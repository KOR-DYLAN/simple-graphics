#include <stdint.h>
#include <string.h>
#include <sgl-core.h>
#include "sgl-osal.h"

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
    size_t size;
} sgl_memory_alignment_t;

typedef union sgl_memory_block sgl_memory_block_t;

union sgl_memory_block {
    struct {
        size_t size;
        sgl_memory_block_t *previous_physical;
        sgl_memory_block_t *previous_free;
        sgl_memory_block_t *next_free;
        uint32_t magic;
        bool is_free;
    } fields;
    sgl_memory_alignment_t alignment;
};

typedef struct {
    uintptr_t begin;
    uintptr_t end;
    sgl_memory_block_t *free_list;
    size_t allocation_count;
    bool is_initialized;
    sgl_osal_mutex_t lock;
} sgl_memory_pool_t;

static sgl_memory_pool_t sgl_memory_pool;

static size_t sgl_memory_align_size(size_t size)
{
    const size_t alignment = sizeof(sgl_memory_alignment_t);
    size_t aligned_size = 0U;

    /* Check the addition before rounding upward to the next alignment unit. */
    if (size <= (SIZE_MAX - (alignment - 1U))) {
        aligned_size = (size + alignment - 1U) & ~(alignment - 1U);
    }

    return aligned_size;
}

static sgl_memory_block_t *sgl_memory_next_physical(sgl_memory_block_t *block)
{
    uintptr_t next;
    sgl_memory_block_t *next_block = NULL;

    /*
     * Physical blocks do not need a next pointer because the next header begins
     * exactly after this header and payload.
     */
    next = (uintptr_t)block + sizeof(sgl_memory_block_t) + block->fields.size;
    if (next < sgl_memory_pool.end) {
        next_block = (sgl_memory_block_t *)next;
    }

    return next_block;
}

static void sgl_memory_remove_free_block(sgl_memory_block_t *block)
{
    /* Detach a block without changing its physical neighbours. */
    if (block->fields.previous_free != NULL) {
        block->fields.previous_free->fields.next_free = block->fields.next_free;
    }
    else {
        sgl_memory_pool.free_list = block->fields.next_free;
    }

    if (block->fields.next_free != NULL) {
        block->fields.next_free->fields.previous_free = block->fields.previous_free;
    }

    block->fields.previous_free = NULL;
    block->fields.next_free = NULL;
}

static void sgl_memory_insert_free_block(sgl_memory_block_t *block)
{
    /* Free-list order is not address order; insertion at the head is O(1). */
    block->fields.is_free = true;
    block->fields.previous_free = NULL;
    block->fields.next_free = sgl_memory_pool.free_list;
    if (sgl_memory_pool.free_list != NULL) {
        sgl_memory_pool.free_list->fields.previous_free = block;
    }
    sgl_memory_pool.free_list = block;
}

static void sgl_memory_split_block(sgl_memory_block_t *block, size_t size)
{
    sgl_memory_block_t *remainder;
    sgl_memory_block_t *next;
    size_t remainder_size;

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
        remainder = (sgl_memory_block_t *)((uintptr_t)block + sizeof(sgl_memory_block_t) + size);
        remainder->fields.size = remainder_size;
        remainder->fields.previous_physical = block;
        remainder->fields.previous_free = NULL;
        remainder->fields.next_free = NULL;
        remainder->fields.magic = SGL_MEMORY_BLOCK_MAGIC;
        remainder->fields.is_free = true;

        block->fields.size = size;
        next = sgl_memory_next_physical(remainder);
        if (next != NULL) {
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
    if ((next != NULL) && (next->fields.magic == SGL_MEMORY_BLOCK_MAGIC) && next->fields.is_free) {
        sgl_memory_remove_free_block(next);
        block->fields.size += sizeof(sgl_memory_block_t) + next->fields.size;
        after_next = sgl_memory_next_physical(block);
        if (after_next != NULL) {
            after_next->fields.previous_physical = block;
        }
    }

    return block;
}

sgl_result_t sgl_memory_pool_initialize(void *memory, size_t size)
{
    uintptr_t raw_address;
    uintptr_t aligned_address;
    size_t alignment;
    size_t offset;
    size_t usable_size;
    sgl_memory_block_t *first;
    sgl_result_t result = SGL_ERROR_INVALID_ARGUMENTS;

    /*
     * Align the beginning inward and the usable size downward. Any bytes lost
     * to alignment remain owned by the caller but are outside the pool.
     */
    if ((memory != NULL) && (sgl_memory_pool.is_initialized == false)) {
        alignment = sizeof(sgl_memory_alignment_t);
        raw_address = (uintptr_t)memory;
        aligned_address = (raw_address + alignment - 1U) & ~(uintptr_t)(alignment - 1U);
        offset = (size_t)(aligned_address - raw_address);
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
            first = (sgl_memory_block_t *)aligned_address;
            first->fields.size = usable_size - sizeof(sgl_memory_block_t);
            first->fields.previous_physical = NULL;
            first->fields.previous_free = NULL;
            first->fields.next_free = NULL;
            first->fields.magic = SGL_MEMORY_BLOCK_MAGIC;
            first->fields.is_free = true;
            sgl_memory_pool.free_list = first;
            sgl_memory_pool.is_initialized = true;
            result = SGL_SUCCESS;
        }
    }

    return result;
}

sgl_result_t sgl_memory_pool_deinitialize(void)
{
    sgl_result_t result = SGL_ERROR_INVALID_ARGUMENTS;

    if (sgl_memory_pool.is_initialized == true) {
        sgl_osal_mutex_lock(&sgl_memory_pool.lock);
        /*
         * Refuse to detach the caller-owned buffer while any returned pointer is
         * still live. After deinitialization the allocator no longer owns or
         * inspects that address range.
         */
        if (sgl_memory_pool.allocation_count == 0U) {
            sgl_memory_pool.is_initialized = false;
            sgl_memory_pool.begin = 0U;
            sgl_memory_pool.end = 0U;
            sgl_memory_pool.free_list = NULL;
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

void *sgl_malloc(size_t size)
{
    sgl_memory_block_t *block = NULL;
    size_t aligned_size = 0U;
    void *memory = NULL;

    if (size != 0U) {
        if (sgl_memory_pool.is_initialized == false) {
            /*
             * A pool is mandatory. Returning NULL keeps the allocator independent
             * from the C runtime heap and makes missing initialization explicit.
             */
            memory = NULL;
        }
        else {
            aligned_size = sgl_memory_align_size(size);
            if (aligned_size != 0U) {
                sgl_osal_mutex_lock(&sgl_memory_pool.lock);
                /* First-fit balances implementation size and predictable cost. */
                block = sgl_memory_pool.free_list;
                while ((block != NULL) && (block->fields.size < aligned_size)) {
                    block = block->fields.next_free;
                }

                if (block != NULL) {
                    sgl_memory_remove_free_block(block);
                    sgl_memory_split_block(block, aligned_size);
                    block->fields.is_free = false;
                    sgl_memory_pool.allocation_count++;
                    memory = (uint8_t *)block + sizeof(sgl_memory_block_t);
                }
                sgl_osal_mutex_unlock(&sgl_memory_pool.lock);
            }
        }
    }

    return memory;
}

void *sgl_calloc(size_t count, size_t size)
{
    size_t total_size;
    void *memory = NULL;

    /* Division-based overflow check avoids evaluating count * size too early. */
    if ((size == 0U) || (count <= (SIZE_MAX / size))) {
        total_size = count * size;
        memory = sgl_malloc(total_size);
        if (memory != NULL) {
            memset(memory, 0, total_size);
        }
    }

    return memory;
}

void sgl_free(void *memory)
{
    sgl_memory_block_t *block;
    sgl_memory_block_t *previous;
    uintptr_t address;

    if (memory != NULL) {
        address = (uintptr_t)memory;
        if ((sgl_memory_pool.is_initialized == true) &&
            (address >= sgl_memory_pool.begin) && (address < sgl_memory_pool.end)) {
            sgl_osal_mutex_lock(&sgl_memory_pool.lock);
            block = (sgl_memory_block_t *)(address - (uintptr_t)sizeof(sgl_memory_block_t));
            if ((block->fields.magic == SGL_MEMORY_BLOCK_MAGIC) && (block->fields.is_free == false)) {
                /*
                 * Merge right first, then left. If the left block is free it is
                 * already in the free list and must be detached before growing.
                 * The final combined block is inserted exactly once.
                 */
                block->fields.is_free = true;
                block = sgl_memory_merge_with_next(block);
                previous = block->fields.previous_physical;
                if ((previous != NULL) && previous->fields.is_free) {
                    sgl_memory_remove_free_block(previous);
                    previous->fields.size += sizeof(sgl_memory_block_t) + block->fields.size;
                    block = previous;
                    previous = sgl_memory_next_physical(block);
                    if (previous != NULL) {
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
