/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#undef LTTNG_UST_TRACEPOINT_PROVIDER
#define LTTNG_UST_TRACEPOINT_PROVIDER sgl_core

#undef LTTNG_UST_TRACEPOINT_INCLUDE
#define LTTNG_UST_TRACEPOINT_INCLUDE "./sgl_lttng_tracepoints.h"

#if !defined(SGL_LTTNG_TRACEPOINTS_H_) || \
    defined(LTTNG_UST_TRACEPOINT_HEADER_MULTI_READ)
#define SGL_LTTNG_TRACEPOINTS_H_

#include <stdint.h>
#include <lttng/tracepoint.h>

LTTNG_UST_TRACEPOINT_EVENT(
    sgl_core,
    resize_begin,
    LTTNG_UST_TP_ARGS(
        const char *, backend,
        const char *, method,
        int32_t, destination_width,
        int32_t, destination_height,
        int32_t, source_width,
        int32_t, source_height,
        int32_t, bytes_per_pixel,
        uint64_t, requested_threads,
        uint8_t, has_prebuilt_lut
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(backend, backend)
        lttng_ust_field_string(method, method)
        lttng_ust_field_integer(int32_t, destination_width,
            destination_width)
        lttng_ust_field_integer(int32_t, destination_height,
            destination_height)
        lttng_ust_field_integer(int32_t, source_width, source_width)
        lttng_ust_field_integer(int32_t, source_height, source_height)
        lttng_ust_field_integer(int32_t, bytes_per_pixel, bytes_per_pixel)
        lttng_ust_field_integer(uint64_t, requested_threads,
            requested_threads)
        lttng_ust_field_integer(uint8_t, has_prebuilt_lut,
            has_prebuilt_lut)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    sgl_core,
    resize_end,
    LTTNG_UST_TP_ARGS(
        const char *, backend,
        const char *, method,
        int32_t, result
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_string(backend, backend)
        lttng_ust_field_string(method, method)
        lttng_ust_field_integer(int32_t, result, result)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    sgl_core,
    threadpool_dispatch_begin,
    LTTNG_UST_TP_ARGS(
        uint64_t, pool,
        uint32_t, generation,
        uint64_t, operation_count,
        uint64_t, worker_limit,
        uint64_t, requested_threads
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer_hex(uint64_t, pool, pool)
        lttng_ust_field_integer(uint32_t, generation, generation)
        lttng_ust_field_integer(uint64_t, operation_count,
            operation_count)
        lttng_ust_field_integer(uint64_t, worker_limit, worker_limit)
        lttng_ust_field_integer(uint64_t, requested_threads,
            requested_threads)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    sgl_core,
    threadpool_participant_begin,
    LTTNG_UST_TP_ARGS(
        uint64_t, pool,
        uint32_t, generation,
        const char *, role
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer_hex(uint64_t, pool, pool)
        lttng_ust_field_integer(uint32_t, generation, generation)
        lttng_ust_field_string(role, role)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    sgl_core,
    threadpool_participant_end,
    LTTNG_UST_TP_ARGS(
        uint64_t, pool,
        uint32_t, generation,
        const char *, role,
        uint64_t, completed_operations
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer_hex(uint64_t, pool, pool)
        lttng_ust_field_integer(uint32_t, generation, generation)
        lttng_ust_field_string(role, role)
        lttng_ust_field_integer(uint64_t, completed_operations,
            completed_operations)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    sgl_core,
    threadpool_completion_wait_begin,
    LTTNG_UST_TP_ARGS(
        uint64_t, pool,
        uint32_t, generation
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer_hex(uint64_t, pool, pool)
        lttng_ust_field_integer(uint32_t, generation, generation)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    sgl_core,
    threadpool_completion_wait_end,
    LTTNG_UST_TP_ARGS(
        uint64_t, pool,
        uint32_t, generation
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer_hex(uint64_t, pool, pool)
        lttng_ust_field_integer(uint32_t, generation, generation)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    sgl_core,
    threadpool_dispatch_end,
    LTTNG_UST_TP_ARGS(
        uint64_t, pool,
        uint32_t, generation,
        uint64_t, operation_count
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer_hex(uint64_t, pool, pool)
        lttng_ust_field_integer(uint32_t, generation, generation)
        lttng_ust_field_integer(uint64_t, operation_count,
            operation_count)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    sgl_core,
    queue_lock_contended,
    LTTNG_UST_TP_ARGS(
        uint64_t, queue,
        const char *, operation
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer_hex(uint64_t, queue, queue)
        lttng_ust_field_string(operation, operation)
    )
)

LTTNG_UST_TRACEPOINT_EVENT(
    sgl_core,
    queue_lock_acquired,
    LTTNG_UST_TP_ARGS(
        uint64_t, queue,
        const char *, operation
    ),
    LTTNG_UST_TP_FIELDS(
        lttng_ust_field_integer_hex(uint64_t, queue, queue)
        lttng_ust_field_string(operation, operation)
    )
)

#endif  /* SGL_LTTNG_TRACEPOINTS_H_ */

#include <lttng/tracepoint-event.h>
