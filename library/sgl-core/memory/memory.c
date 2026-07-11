/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
/*
 * SGL-MEM-DEV-002: allocator metadata uses unions to guarantee alignment and
 * integer address conversion to validate and navigate its caller-owned byte
 * pool. These operations are confined to the address conversion helpers in
 * this implementation.
 */
#include <sgl-core.h>
#include "sgl-osal.h"
#include <sgl_memory_cast.h>

#define SGL_MEMORY_BLOCK_MAGIC          (0x53474C4DU)
#define SGL_MEMORY_MIN_PAYLOAD_SIZE     (sizeof(sgl_memory_alignment_t))

/*
 * Design overview
 * ---------------
 * The caller supplies one contiguous memory region. The allocator divides that
 * region into variable-size blocks. Every block stores its metadata immediately
 * before the address returned to the caller.
 *
 * Pool address order:
 *
 *   begin                                                        end
 *     |                                                            |
 *     v                                                            v
 *   +----------+----------------+----------+---------+----------+---+
 *   | header A | payload A      | header B | free B  | header C |...|
 *   +----------+----------------+----------+---------+----------+---+
 *                ^                         ^
 *                returned pointer          possible allocation
 *
 * There are two relationships between blocks:
 *
 *   1. Physical order
 *      previous_physical points to the immediately preceding block in the pool.
 *      The next physical block is calculated from header size + payload size.
 *      These links allow adjacent free blocks to be coalesced.
 *
 *   2. Free-list order
 *      previous_free/next_free form a doubly linked list containing only free
 *      blocks. Allocation performs a first-fit search through this list.
 *
 * Allocation and release modify shared block metadata, so each complete
 * operation is serialized by the pool mutex. Pool initialization and
 * deinitialization must be performed while no other thread is using the
 * allocator.
 */
typedef union {
    /*
     * The union makes both block headers and returned payload addresses suitably
     * aligned for the fundamental scalar types supported by the target.
     */
    long double value;
    void *pointer;
    sgl_size_t size;
} sgl_memory_alignment_t;

typedef union sgl_memory_block sgl_memory_block_t;

union sgl_memory_block {
    struct {
        sgl_size_t size;
        sgl_memory_block_t *previous_physical;
        sgl_memory_block_t *previous_free;
        sgl_memory_block_t *next_free;
        sgl_uint32_t magic;
        sgl_bool_t is_free;
    } fields;
    sgl_memory_alignment_t alignment;
};

typedef struct {
    sgl_uintptr_t begin;
    sgl_uintptr_t end;
    sgl_memory_block_t *free_list;
    sgl_size_t allocation_count;
    sgl_bool_t is_initialized;
    sgl_osal_mutex_t lock;
} sgl_memory_pool_t;

static sgl_memory_pool_t sgl_memory_pool;

static SGL_ALWAYS_INLINE sgl_uintptr_t sgl_memory_address_from_void(const void *memory)
{
    sgl_uintptr_t result;

    /*
     * SGL-MEM-DEV-002:
     * Pool validation compares addresses numerically before the allocator
     * recovers any block metadata from the caller-provided storage.
     */
    /* cppcheck-suppress misra-c2012-11.6 */
    result = (sgl_uintptr_t)memory;

    return result;
}

static SGL_ALWAYS_INLINE sgl_uintptr_t sgl_memory_address_from_block(const sgl_memory_block_t *block)
{
    sgl_uintptr_t result;

    /*
     * SGL-MEM-DEV-002:
     * Physical block navigation is address arithmetic inside the active pool.
     */
    /* cppcheck-suppress misra-c2012-11.4 */
    result = (sgl_uintptr_t)block;

    return result;
}

static SGL_ALWAYS_INLINE sgl_memory_block_t *sgl_memory_block_from_address(sgl_uintptr_t address)
{
    sgl_memory_block_t *result;

    /*
     * SGL-MEM-DEV-002:
     * The allocator only rebuilds block pointers from addresses that were
     * derived from the initialized pool range.
     */
    /* cppcheck-suppress misra-c2012-11.4 */
    result = (sgl_memory_block_t *)address;

    return result;
}

static sgl_size_t sgl_memory_align_size(sgl_size_t size)
{
    const sgl_size_t alignment = sizeof(sgl_memory_alignment_t);
    sgl_size_t aligned_size = 0U;

    /* Check the addition before rounding upward to the next alignment unit. */
    if (size <= (SGL_SIZE_MAX - (alignment - 1U))) {
        aligned_size = (size + alignment - 1U) & ~(alignment - 1U);
    }

    return aligned_size;
}

static sgl_memory_block_t *sgl_memory_next_physical(sgl_memory_block_t *block)
{
    sgl_uintptr_t next;
    sgl_memory_block_t *next_block = SGL_NULL;

    /*
     * Physical blocks do not need a next pointer because the next header begins
     * exactly after this header and payload.
     */
    next = sgl_memory_address_from_block(block) + sizeof(sgl_memory_block_t) + block->fields.size;
    if (next < sgl_memory_pool.end) {
        next_block = sgl_memory_block_from_address(next);
    }

    return next_block;
}

static void sgl_memory_remove_free_block(sgl_memory_block_t *block)
{
    /* Detach a block without changing its physical neighbours. */
    if (block->fields.previous_free != SGL_NULL) {
        block->fields.previous_free->fields.next_free = block->fields.next_free;
    }
    else {
        sgl_memory_pool.free_list = block->fields.next_free;
    }

    if (block->fields.next_free != SGL_NULL) {
        block->fields.next_free->fields.previous_free = block->fields.previous_free;
    }

    block->fields.previous_free = SGL_NULL;
    block->fields.next_free = SGL_NULL;
}

static void sgl_memory_insert_free_block(sgl_memory_block_t *block)
{
    /* Free-list order is not address order; insertion at the head is O(1). */
    block->fields.is_free = SGL_TRUE;
    block->fields.previous_free = SGL_NULL;
    block->fields.next_free = sgl_memory_pool.free_list;
    if (sgl_memory_pool.free_list != SGL_NULL) {
        sgl_memory_pool.free_list->fields.previous_free = block;
    }
    sgl_memory_pool.free_list = block;
}

static void sgl_memory_split_block(sgl_memory_block_t *block, sgl_size_t size)
{
    sgl_memory_block_t *remainder;
    sgl_memory_block_t *next;
    sgl_size_t remainder_size;

    /*
     * Split only when the remainder can contain another header and at least one
     * minimally aligned payload.
     *
     * Before:
     *   +--------+---------------------------------------+
     *   | header |             free payload              |
     *   +--------+---------------------------------------+
     *
     * After:
     *   +--------+---------------+--------+--------------+
     *   | header | requested     | header | free rest    |
     *   +--------+---------------+--------+--------------+
     */
    if (block->fields.size >= (size + sizeof(sgl_memory_block_t) + SGL_MEMORY_MIN_PAYLOAD_SIZE)) {
        remainder_size = block->fields.size - size - sizeof(sgl_memory_block_t);
        remainder = sgl_memory_block_from_address(
            sgl_memory_address_from_block(block) + sizeof(sgl_memory_block_t) + size);
        remainder->fields.size = remainder_size;
        remainder->fields.previous_physical = block;
        remainder->fields.previous_free = SGL_NULL;
        remainder->fields.next_free = SGL_NULL;
        remainder->fields.magic = SGL_MEMORY_BLOCK_MAGIC;
        remainder->fields.is_free = SGL_TRUE;

        block->fields.size = size;
        next = sgl_memory_next_physical(remainder);
        if (next != SGL_NULL) {
            next->fields.previous_physical = remainder;
        }

        sgl_memory_insert_free_block(remainder);
    }
}

static sgl_memory_block_t *sgl_memory_merge_with_next(sgl_memory_block_t *block)
{
    sgl_memory_block_t *next;
    sgl_memory_block_t *after_next;

    /*
     * Coalesce only physically adjacent blocks. The absorbed block is removed
     * from the free list before its header becomes part of the merged payload.
     *
     *   +--------+---------+--------+---------+
     *   | block  | free    | next   | free    |
     *   +--------+---------+--------+---------+
     *                    becomes
     *   +--------+----------------------------+
     *   | block  |            free            |
     *   +--------+----------------------------+
     */
    next = sgl_memory_next_physical(block);
    if ((next != SGL_NULL) && (next->fields.magic == SGL_MEMORY_BLOCK_MAGIC) && next->fields.is_free) {
        sgl_memory_remove_free_block(next);
        block->fields.size += sizeof(sgl_memory_block_t) + next->fields.size;
        after_next = sgl_memory_next_physical(block);
        if (after_next != SGL_NULL) {
            after_next->fields.previous_physical = block;
        }
    }

    return block;
}

sgl_result_t sgl_memory_pool_initialize(void *memory, sgl_size_t size)
{
    sgl_uintptr_t raw_address;
    sgl_uintptr_t aligned_address;
    sgl_size_t alignment;
    sgl_size_t offset;
    sgl_size_t usable_size;
    sgl_memory_block_t *first;
    sgl_result_t result = SGL_ERROR_INVALID_ARGUMENTS;

    /*
     * Align the beginning inward and the usable size downward. Any bytes lost
     * to alignment remain owned by the caller but are outside the pool.
     */
    if ((memory != SGL_NULL) && (sgl_memory_pool.is_initialized == SGL_FALSE)) {
        alignment = sizeof(sgl_memory_alignment_t);
        raw_address = sgl_memory_address_from_void(memory);
        aligned_address = (raw_address + alignment - 1U) & ~(sgl_uintptr_t)(alignment - 1U);
        offset = (sgl_size_t)(aligned_address - raw_address);
        if ((size > offset) &&
            ((size - offset) >= (sizeof(sgl_memory_block_t) + SGL_MEMORY_MIN_PAYLOAD_SIZE))) {
            usable_size = (size - offset) & ~(alignment - 1U);
            sgl_memory_pool.begin = aligned_address;
            sgl_memory_pool.end = sgl_memory_pool.begin + usable_size;
            sgl_memory_pool.allocation_count = 0U;
            sgl_osal_mutex_init(&sgl_memory_pool.lock);

            /*
             * Initialization creates one free block spanning the entire usable
             * region. Later allocations progressively split this block.
             */
            first = sgl_memory_block_from_address(aligned_address);
            first->fields.size = usable_size - sizeof(sgl_memory_block_t);
            first->fields.previous_physical = SGL_NULL;
            first->fields.previous_free = SGL_NULL;
            first->fields.next_free = SGL_NULL;
            first->fields.magic = SGL_MEMORY_BLOCK_MAGIC;
            first->fields.is_free = SGL_TRUE;
            sgl_memory_pool.free_list = first;
            sgl_memory_pool.is_initialized = SGL_TRUE;
            result = SGL_SUCCESS;
        }
    }

    return result;
}

sgl_result_t sgl_memory_pool_deinitialize(void)
{
    sgl_result_t result = SGL_ERROR_INVALID_ARGUMENTS;

    if (sgl_memory_pool.is_initialized == SGL_TRUE) {
        sgl_osal_mutex_lock(&sgl_memory_pool.lock);
        /*
         * Refuse to detach the caller-owned buffer while any returned pointer is
         * still live. After deinitialization the allocator no longer owns or
         * inspects that address range.
         */
        if (sgl_memory_pool.allocation_count == 0U) {
            sgl_memory_pool.is_initialized = SGL_FALSE;
            sgl_memory_pool.begin = 0U;
            sgl_memory_pool.end = 0U;
            sgl_memory_pool.free_list = SGL_NULL;
            result = SGL_SUCCESS;
        }
        else {
            result = SGL_FAILURE;
        }
        sgl_osal_mutex_unlock(&sgl_memory_pool.lock);
        if (result == SGL_SUCCESS) {
            sgl_osal_mutex_destroy(&sgl_memory_pool.lock);
        }
    }

    return result;
}

void *sgl_malloc(sgl_size_t size)
{
    sgl_memory_block_t *block = SGL_NULL;
    sgl_size_t aligned_size = 0U;
    void *memory = SGL_NULL;

    if (size != 0U) {
        if (sgl_memory_pool.is_initialized == SGL_FALSE) {
            /*
             * A pool is mandatory. Returning SGL_NULL keeps the allocator independent
             * from the C runtime heap and makes missing initialization explicit.
             */
            memory = SGL_NULL;
        }
        else {
            aligned_size = sgl_memory_align_size(size);
            if (aligned_size != 0U) {
                sgl_osal_mutex_lock(&sgl_memory_pool.lock);
                /* First-fit balances implementation size and predictable cost. */
                block = sgl_memory_pool.free_list;
                while ((block != SGL_NULL) && (block->fields.size < aligned_size)) {
                    block = block->fields.next_free;
                }

                if (block != SGL_NULL) {
                    sgl_memory_remove_free_block(block);
                    sgl_memory_split_block(block, aligned_size);
                    block->fields.is_free = SGL_FALSE;
                    sgl_memory_pool.allocation_count++;
                    memory = &sgl_memory_as_uint8(block)[sizeof(sgl_memory_block_t)];
                }
                sgl_osal_mutex_unlock(&sgl_memory_pool.lock);
            }
        }
    }

    return memory;
}

void *sgl_calloc(sgl_size_t count, sgl_size_t size)
{
    sgl_size_t total_size;
    void *memory = SGL_NULL;

    /* Division-based overflow check avoids evaluating count * size too early. */
    if ((size == 0U) || (count <= (SGL_SIZE_MAX / size))) {
        total_size = count * size;
        memory = sgl_malloc(total_size);
        if (memory != SGL_NULL) {
            (void)sgl_memset(memory, 0, total_size);
        }
    }

    return memory;
}

void sgl_free(void *memory)
{
    sgl_memory_block_t *block;
    sgl_memory_block_t *previous;
    sgl_uintptr_t address;

    if (memory != SGL_NULL) {
        address = sgl_memory_address_from_void(memory);
        if ((sgl_memory_pool.is_initialized == SGL_TRUE) &&
            (address >= sgl_memory_pool.begin) && (address < sgl_memory_pool.end)) {
            sgl_osal_mutex_lock(&sgl_memory_pool.lock);
            block = sgl_memory_block_from_address(address - (sgl_uintptr_t)sizeof(sgl_memory_block_t));
            if ((block->fields.magic == SGL_MEMORY_BLOCK_MAGIC) && (block->fields.is_free == SGL_FALSE)) {
                /*
                 * Merge right first, then left. If the left block is free it is
                 * already in the free list and must be detached before growing.
                 * The final combined block is inserted exactly once.
                 */
                block->fields.is_free = SGL_TRUE;
                block = sgl_memory_merge_with_next(block);
                previous = block->fields.previous_physical;
                if ((previous != SGL_NULL) && previous->fields.is_free) {
                    sgl_memory_remove_free_block(previous);
                    previous->fields.size += sizeof(sgl_memory_block_t) + block->fields.size;
                    block = previous;
                    previous = sgl_memory_next_physical(block);
                    if (previous != SGL_NULL) {
                        previous->fields.previous_physical = block;
                    }
                }
                sgl_memory_insert_free_block(block);
                sgl_memory_pool.allocation_count--;
            }
            sgl_osal_mutex_unlock(&sgl_memory_pool.lock);
        }
    }
}
