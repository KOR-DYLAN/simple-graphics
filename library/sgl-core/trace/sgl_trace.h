/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef SGL_TRACE_H_
#define SGL_TRACE_H_

#include <sgl-config.h>
#include <sgl-compiler.h>
#include <sgl-core.h>
#include <sgl-type.h>

#define SGL_TRACE_BACKEND_GENERIC       "generic"
#define SGL_TRACE_BACKEND_SIMD          "simd"
#define SGL_TRACE_METHOD_NEAREST        "nearest"
#define SGL_TRACE_METHOD_BILINEAR       "bilinear"
#define SGL_TRACE_METHOD_BICUBIC        "bicubic"
#define SGL_TRACE_ROLE_SUBMITTER        "submitter"
#define SGL_TRACE_ROLE_WORKER           "worker"
#define SGL_TRACE_QUEUE_ENQUEUE         "enqueue"
#define SGL_TRACE_QUEUE_DEQUEUE         "dequeue"
#define SGL_TRACE_QUEUE_PEEK            "peek"

#if defined(SGL_CFG_HAS_LTTNG) && defined(SGL_CFG_HAS_THREAD)
static SGL_ALWAYS_INLINE sgl_size_t sgl_trace_requested_threads(
    const sgl_threadpool_t *pool)
{
    sgl_size_t thread_count;

    thread_count = sgl_threadpool_get_num_threads(pool);
    if (thread_count == 0U) {
        thread_count = 1U;
    }

    return thread_count;
}

#define SGL_TRACE_REQUESTED_THREADS(pool) \
    sgl_trace_requested_threads(pool)
#else
#define SGL_TRACE_REQUESTED_THREADS(pool)    (1U)
#endif

#if defined(SGL_CFG_HAS_LTTNG)
#include "sgl_lttng_tracepoints.h"

static SGL_ALWAYS_INLINE sgl_uint64_t sgl_trace_pointer_id(
    const void *pointer)
{
    sgl_uintptr_t pointer_value;

    /* Trace correlation records pointer identity without dereferencing it. */
    /* cppcheck-suppress misra-c2012-11.6 */
    pointer_value = (sgl_uintptr_t)pointer;

    return (sgl_uint64_t)pointer_value;
}

#define SGL_TRACE_RESIZE_BEGIN(backend, method, d_width, d_height, \
                               s_width, s_height, bpp, threads, has_lut) \
    lttng_ust_tracepoint(sgl_core, resize_begin, \
        backend, method, d_width, d_height, s_width, s_height, bpp, \
        (sgl_uint64_t)(threads), (sgl_uint8_t)(has_lut))
#define SGL_TRACE_RESIZE_END(backend, method, result) \
    lttng_ust_tracepoint(sgl_core, resize_end, backend, method, result)
#define SGL_TRACE_THREADPOOL_DISPATCH_BEGIN(pool, generation, operations, \
                                            workers, threads) \
    lttng_ust_tracepoint(sgl_core, threadpool_dispatch_begin, \
        sgl_trace_pointer_id(pool), generation, (sgl_uint64_t)(operations), \
        (sgl_uint64_t)(workers), (sgl_uint64_t)(threads))
#define SGL_TRACE_THREADPOOL_PARTICIPANT_BEGIN(pool, generation, role) \
    lttng_ust_tracepoint(sgl_core, threadpool_participant_begin, \
        sgl_trace_pointer_id(pool), generation, role)
#define SGL_TRACE_THREADPOOL_PARTICIPANT_END(pool, generation, role, count) \
    lttng_ust_tracepoint(sgl_core, threadpool_participant_end, \
        sgl_trace_pointer_id(pool), generation, role, (sgl_uint64_t)(count))
#define SGL_TRACE_THREADPOOL_COMPLETION_WAIT_BEGIN(pool, generation) \
    lttng_ust_tracepoint(sgl_core, threadpool_completion_wait_begin, \
        sgl_trace_pointer_id(pool), generation)
#define SGL_TRACE_THREADPOOL_COMPLETION_WAIT_END(pool, generation) \
    lttng_ust_tracepoint(sgl_core, threadpool_completion_wait_end, \
        sgl_trace_pointer_id(pool), generation)
#define SGL_TRACE_THREADPOOL_DISPATCH_END(pool, generation, operations) \
    lttng_ust_tracepoint(sgl_core, threadpool_dispatch_end, \
        sgl_trace_pointer_id(pool), generation, (sgl_uint64_t)(operations))
#define SGL_TRACE_QUEUE_LOCK_CONTENDED(queue, operation) \
    lttng_ust_tracepoint(sgl_core, queue_lock_contended, \
        sgl_trace_pointer_id(queue), operation)
#define SGL_TRACE_QUEUE_LOCK_ACQUIRED(queue, operation) \
    lttng_ust_tracepoint(sgl_core, queue_lock_acquired, \
        sgl_trace_pointer_id(queue), operation)
#else
#define SGL_TRACE_RESIZE_BEGIN(...)                         ((void)0)
#define SGL_TRACE_RESIZE_END(...)                           ((void)0)
#define SGL_TRACE_THREADPOOL_DISPATCH_BEGIN(...)            ((void)0)
#define SGL_TRACE_THREADPOOL_PARTICIPANT_BEGIN(...)         ((void)0)
#define SGL_TRACE_THREADPOOL_PARTICIPANT_END(...)           ((void)0)
#define SGL_TRACE_THREADPOOL_COMPLETION_WAIT_BEGIN(...)     ((void)0)
#define SGL_TRACE_THREADPOOL_COMPLETION_WAIT_END(...)       ((void)0)
#define SGL_TRACE_THREADPOOL_DISPATCH_END(...)              ((void)0)
#define SGL_TRACE_QUEUE_LOCK_CONTENDED(...)                 ((void)0)
#define SGL_TRACE_QUEUE_LOCK_ACQUIRED(...)                  ((void)0)
#endif

#endif  /* SGL_TRACE_H_ */
