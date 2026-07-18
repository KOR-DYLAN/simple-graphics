/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#include <sgl-core.h>
#include "nearest_neighbor.h"
#include "sgl_trace.h"
#include "threaded_resize.h"

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_generic_resize_nearest_neighbor_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie);
#endif  /* !SGL_CFG_HAS_THREAD */

static sgl_int32_t sgl_generic_resize_nearest_count_errors(
    const sgl_uint8_t *dst,
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    const sgl_uint8_t *src,
    sgl_int32_t s_width,
    sgl_int32_t s_height,
    sgl_int32_t bpp)
{
    sgl_int32_t errcnt;

    errcnt = 0;

    /* check buffer address */
    if ((dst == SGL_NULL) || (src == SGL_NULL)) {
        errcnt += 1;
    }

    /* check boundary */
    if ((d_width <= 0) || (d_height <= 0) || (s_width <= 0) || (s_height <= 0)) {
        errcnt += 1;
    }

    /* check bpp(bytes per pixel) */
    if (bpp <= 0) {
        errcnt += 1;
    }

    return errcnt;
}

static SGL_ALWAYS_INLINE sgl_bool_t sgl_generic_resize_nearest_is_same_size(
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_int32_t s_width,
    sgl_int32_t s_height)
{
    sgl_bool_t result;

    result = SGL_FALSE;
    if ((d_width == s_width) && (d_height == s_height)) {
        result = SGL_TRUE;
    }

    return result;
}

static SGL_ALWAYS_INLINE void sgl_generic_resize_nearest_copy_same_size(
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t bpp)
{
    sgl_size_t copy_size;

    copy_size = (sgl_size_t)d_width * (sgl_size_t)d_height * (sgl_size_t)bpp;
    if (dst != src) {
        (void)sgl_memcpy(dst, src, copy_size);
    }
}

static sgl_nearest_neighbor_lookup_t *sgl_generic_resize_nearest_select_lut(
    sgl_nearest_neighbor_lookup_t *SGL_RESTRICT ext_lut,
    sgl_nearest_neighbor_lookup_t **temp_lut,
    sgl_int32_t d_width,
    sgl_int32_t d_height,
    sgl_int32_t s_width,
    sgl_int32_t s_height)
{
    sgl_nearest_neighbor_lookup_t *lut;

    lut = SGL_NULL;
    if (ext_lut != SGL_NULL) {
        if ((ext_lut->d_width == d_width) && (ext_lut->d_height == d_height) &&
            (ext_lut->s_width == s_width) && (ext_lut->s_height == s_height))
        {
            /* apply external look-up table */
            lut = ext_lut;
        }
    }

    if (lut == SGL_NULL) {
        /* create temp look-up table */
        *temp_lut = sgl_generic_create_nearest_neighbor_lut(
            d_width, d_height, s_width, s_height);
        lut = *temp_lut;
    }

    return lut;
}

static SGL_ALWAYS_INLINE void sgl_generic_resize_nearest_set_data(
    sgl_nearest_neighbor_data_t *data,
    sgl_nearest_neighbor_lookup_t *lut,
    sgl_uint8_t *SGL_RESTRICT dst,
    sgl_uint8_t *SGL_RESTRICT src,
    sgl_int32_t s_width,
    sgl_int32_t d_width,
    sgl_int32_t bpp)
{
    data->bpp = bpp;
    data->src = src;
    data->dst = dst;
    data->lut = lut;
    data->src_stride = s_width * bpp;
    data->dst_stride = d_width * bpp;
}

static SGL_ALWAYS_INLINE void sgl_generic_resize_nearest_single(
    sgl_nearest_neighbor_data_t *data,
    sgl_int32_t d_height)
{
    sgl_resize_nearest_neighbor_dispatch_packed_range(0, d_height, data);
}

#if defined(SGL_CFG_HAS_THREAD)
static sgl_result_t sgl_generic_resize_nearest_threaded(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_nearest_neighbor_data_t *data,
    sgl_int32_t d_height)
{
    sgl_result_t result;
    sgl_nearest_neighbor_current_t *currents;
    sgl_queue_t *operations;
    sgl_int32_t i;
    sgl_int32_t num_operations;
    sgl_int32_t mod_operations;
    sgl_int32_t bulk_size;

    result = SGL_ERROR_MEMORY_ALLOCATION;
    currents = SGL_NULL;
    operations = SGL_NULL;
    bulk_size = sgl_resize_uniform_thread_bulk_size(
        pool, d_height, SGL_GENERIC_BULK_SIZE);
    num_operations = d_height / bulk_size;
    mod_operations = d_height % bulk_size;
    if (mod_operations != 0) {
        num_operations += 1;
    }

    operations = sgl_queue_create((sgl_size_t)num_operations);
    currents = sgl_memory_as_nearest_neighbor_current(sgl_malloc(
        sizeof(sgl_nearest_neighbor_current_t) * (sgl_size_t)num_operations));
    if ((operations != SGL_NULL) && (currents != SGL_NULL)) {
        for (i = 0; i < num_operations; ++i) {
            currents[i].row = i * bulk_size;
            currents[i].count = bulk_size;
            (void)sgl_queue_unsafe_enqueue(operations, (const void *)&currents[i]);
        }

        if (mod_operations != 0) {
            currents[num_operations - 1].count = mod_operations;
        }

        /* multi-threaded resize */
        result = sgl_threadpool_attach_routine_consuming(
            pool,
            sgl_generic_resize_nearest_neighbor_routine,
            operations,
            (void *)data);
        sgl_queue_destroy(&operations);
    }
    SGL_SAFE_FREE(currents);
    SGL_SAFE_FREE(operations);

    return result;
}
#endif  /* !SGL_CFG_HAS_THREAD */

static sgl_result_t sgl_generic_resize_nearest_run(
    sgl_threadpool_t *SGL_RESTRICT pool,
    sgl_nearest_neighbor_data_t *data,
    sgl_int32_t d_height)
{
    sgl_result_t result;

    result = SGL_SUCCESS;
    if (pool == SGL_NULL) {
        sgl_generic_resize_nearest_single(data, d_height);
    }
#if defined(SGL_CFG_HAS_THREAD)
    else if (sgl_resize_nearest_should_use_threadpool(
                 pool, data->lut->d_width, d_height,
                 data->bpp) == SGL_FALSE) {
        sgl_generic_resize_nearest_single(data, d_height);
    }
    else {
        result = sgl_generic_resize_nearest_threaded(pool, data, d_height);
    }
#else
    else {
        result = SGL_ERROR_NOT_SUPPORTED;
    }
#endif  /* !SGL_CFG_HAS_THREAD */

    return result;
}

sgl_result_t sgl_generic_resize_nearest(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_nearest_neighbor_lookup_t *SGL_RESTRICT ext_lut,
                sgl_uint8_t *SGL_RESTRICT dst, sgl_int32_t d_width, sgl_int32_t d_height,
                sgl_uint8_t *SGL_RESTRICT src, sgl_int32_t s_width, sgl_int32_t s_height,
                sgl_int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;
    sgl_nearest_neighbor_data_t data;
    sgl_nearest_neighbor_lookup_t *lut = SGL_NULL;
    sgl_nearest_neighbor_lookup_t *temp_lut = SGL_NULL;
    sgl_int32_t errcnt = 0;

    SGL_TRACE_RESIZE_BEGIN(
        SGL_TRACE_BACKEND_GENERIC,
        SGL_TRACE_METHOD_NEAREST,
        d_width,
        d_height,
        s_width,
        s_height,
        bpp,
        SGL_TRACE_REQUESTED_THREADS(pool),
        (ext_lut != SGL_NULL));
    errcnt = sgl_generic_resize_nearest_count_errors(
        dst, d_width, d_height, src, s_width, s_height, bpp);

    /* check error count */
    if (errcnt != 0) {
        result = SGL_ERROR_INVALID_ARGUMENTS;
    }
    else if (sgl_generic_resize_nearest_is_same_size(
                 d_width, d_height, s_width, s_height) == SGL_TRUE) {
        sgl_generic_resize_nearest_copy_same_size(dst, d_width, d_height, src, bpp);
    }
    else {
        lut = sgl_generic_resize_nearest_select_lut(
            ext_lut, &temp_lut, d_width, d_height, s_width, s_height);
        if (lut != SGL_NULL) {
            sgl_generic_resize_nearest_set_data(
                &data, lut, dst, src, s_width, d_width, bpp);
            result = sgl_generic_resize_nearest_run(pool, &data, d_height);

            if (temp_lut != SGL_NULL) {
                /* destroy temp look-up table */
                sgl_generic_destroy_nearest_neighbor_lut(temp_lut);
            }
        }
    }

    SGL_TRACE_RESIZE_END(
        SGL_TRACE_BACKEND_GENERIC, SGL_TRACE_METHOD_NEAREST, result);

    return result;
}

#if defined(SGL_CFG_HAS_THREAD)
static void sgl_generic_resize_nearest_neighbor_routine(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie)
{
    const sgl_nearest_neighbor_current_t *cur = sgl_memory_as_const_nearest_neighbor_current(current);
    sgl_nearest_neighbor_data_t *data = sgl_memory_as_nearest_neighbor_data(cookie);

    sgl_resize_nearest_neighbor_dispatch_packed_range(
        cur->row, cur->count, data);
}
#endif  /* !SGL_CFG_HAS_THREAD */
