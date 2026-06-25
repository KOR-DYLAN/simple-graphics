#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sgl-core.h>

#if defined(SGL_CFG_HAS_PTHREAD)
#include <pthread.h>
#endif

#define TEST_POOL_SIZE                  (256U * 1024U)
#define TEST_POOL_GUARD_SIZE            (64U)
#define TEST_POOL_GUARD_VALUE           (0xD3U)
#define TEST_SEQUENTIAL_SLOT_COUNT      (128U)
#define TEST_SEQUENTIAL_ITERATIONS      (100000U)
#define TEST_FRAGMENT_BLOCK_COUNT       (192U)
#define TEST_EXHAUST_BLOCK_COUNT        (2048U)
#define TEST_THREAD_COUNT               (8U)
#define TEST_THREAD_SLOT_COUNT          (64U)
#define TEST_THREAD_ITERATIONS          (50000U)
#define TEST_HANDOFF_BLOCK_COUNT        (512U)

typedef struct {
    unsigned char *memory;
    size_t size;
    uint32_t signature;
} test_allocation_t;

typedef int (*test_case_t)(void);

/*
 * One byte is deliberately added before the pool address. This forces the
 * allocator to exercise its internal alignment adjustment. Bytes outside the
 * supplied pool range are guards and must never change.
 *
 *   +--------------+----------------------+--------------+
 *   | prefix guard | unaligned pool range | suffix guard |
 *   +--------------+----------------------+--------------+
 */
static unsigned char test_pool_storage[
    TEST_POOL_GUARD_SIZE + 1U + TEST_POOL_SIZE + TEST_POOL_GUARD_SIZE];

static uint32_t test_random_next(uint32_t *state)
{
    uint32_t value;

    value = *state;
    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;
    *state = value;

    return value;
}

static unsigned char test_pattern_byte(uint32_t signature, size_t index)
{
    uint32_t mixed;

    mixed = signature ^ (uint32_t)index;
    mixed ^= (uint32_t)(index >> 8U);
    mixed *= 0x45D9F3BU;
    mixed ^= mixed >> 16U;

    return (unsigned char)mixed;
}

static void test_fill_allocation(test_allocation_t *allocation)
{
    size_t i;

    for (i = 0U; i < allocation->size; ++i) {
        allocation->memory[i] = test_pattern_byte(allocation->signature, i);
    }
}

static int test_verify_allocation(const test_allocation_t *allocation)
{
    size_t i;
    int result = 0;

    if ((allocation->memory == NULL) || (allocation->size == 0U)) {
        result = 1;
    }
    else {
        for (i = 0U; (i < allocation->size) && (result == 0); ++i) {
            if (allocation->memory[i] != test_pattern_byte(allocation->signature, i)) {
                result = 1;
            }
        }
    }

    return result;
}

static int test_verify_zero(const unsigned char *memory, size_t size)
{
    size_t i;
    int result = 0;

    for (i = 0U; (i < size) && (result == 0); ++i) {
        if (memory[i] != 0U) {
            result = 1;
        }
    }

    return result;
}

static void test_release_allocation(test_allocation_t *allocation)
{
    sgl_free(allocation->memory);
    allocation->memory = NULL;
    allocation->size = 0U;
    allocation->signature = 0U;
}

static int test_verify_all_active(
    const test_allocation_t *allocations,
    size_t allocation_count)
{
    size_t i;
    int result = 0;

    for (i = 0U; (i < allocation_count) && (result == 0); ++i) {
        if ((allocations[i].memory != NULL) &&
            (test_verify_allocation(&allocations[i]) != 0)) {
            result = 1;
        }
    }

    return result;
}

static void test_prepare_pool_storage(void)
{
    memset(test_pool_storage, TEST_POOL_GUARD_VALUE, sizeof(test_pool_storage));
}

static int test_verify_pool_guards(void)
{
    size_t suffix_begin;
    size_t i;
    int result = 0;

    suffix_begin = TEST_POOL_GUARD_SIZE + 1U + TEST_POOL_SIZE;
    for (i = 0U; (i < (TEST_POOL_GUARD_SIZE + 1U)) && (result == 0); ++i) {
        if (test_pool_storage[i] != TEST_POOL_GUARD_VALUE) {
            result = 1;
        }
    }
    for (i = suffix_begin; (i < sizeof(test_pool_storage)) && (result == 0); ++i) {
        if (test_pool_storage[i] != TEST_POOL_GUARD_VALUE) {
            result = 1;
        }
    }

    return result;
}

static int test_uninitialized_pool_contract(void)
{
    unsigned char *memory = NULL;
    int result = 0;

    memory = (unsigned char *)sgl_calloc(257U, sizeof(unsigned char));
    if (memory != NULL) {
        result = 1;
    }
    if (sgl_malloc(1U) != NULL) {
        result = 1;
    }
    /* NULL and non-pool pointers are ignored when no pool is active. */
    sgl_free(memory);

    return result;
}

static int test_basic_contract(void)
{
    test_allocation_t allocation = { NULL, 0U, 0U };
    unsigned char *zeroed = NULL;
    sgl_queue_t *queue = NULL;
    sgl_nearest_neighbor_lookup_t *lookup = NULL;
    int result = 0;

    allocation.size = 37U;
    allocation.signature = 0xA51C93E7U;
    allocation.memory = (unsigned char *)sgl_malloc(allocation.size);
    zeroed = (unsigned char *)sgl_calloc(127U, 3U);
    if ((allocation.memory == NULL) || (zeroed == NULL)) {
        result = 1;
    }
    if (result == 0) {
        test_fill_allocation(&allocation);
        if ((test_verify_allocation(&allocation) != 0) ||
            (test_verify_zero(zeroed, 381U) != 0)) {
            result = 1;
        }
    }
    if ((result == 0) && (sgl_calloc(SIZE_MAX, 2U) != NULL)) {
        result = 1;
    }
    if ((result == 0) &&
        (sgl_memory_pool_initialize(test_pool_storage, TEST_POOL_SIZE) !=
         SGL_ERROR_INVALID_ARGUMENTS)) {
        result = 1;
    }
    if ((result == 0) && (sgl_memory_pool_deinitialize() != SGL_FAILURE)) {
        result = 1;
    }

    test_release_allocation(&allocation);
    sgl_free(zeroed);

    if (result == 0) {
        queue = sgl_queue_create(64U);
        lookup = sgl_generic_create_nearest_neighbor_lut(320, 240, 640, 480);
        if ((queue == NULL) || (lookup == NULL)) {
            result = 1;
        }
    }
    sgl_queue_destroy(&queue);
    sgl_generic_destroy_nearest_neighbor_lut(lookup);

    return result;
}

static int test_fragmentation_and_coalescing(void)
{
    test_allocation_t blocks[TEST_FRAGMENT_BLOCK_COUNT] = { { NULL, 0U, 0U } };
    test_allocation_t large = { NULL, 0U, 0U };
    size_t i;
    int result = 0;

    for (i = 0U; (i < TEST_FRAGMENT_BLOCK_COUNT) && (result == 0); ++i) {
        blocks[i].size = 31U + ((i * 97U) % 701U);
        blocks[i].signature = 0x20000000U + (uint32_t)i;
        blocks[i].memory = (unsigned char *)sgl_malloc(blocks[i].size);
        if (blocks[i].memory == NULL) {
            result = 1;
        }
        else {
            test_fill_allocation(&blocks[i]);
        }
    }

    /* Create alternating holes while continuously checking surviving blocks. */
    for (i = 1U; i < TEST_FRAGMENT_BLOCK_COUNT; i += 2U) {
        if ((result == 0) && (test_verify_allocation(&blocks[i]) != 0)) {
            result = 1;
        }
        test_release_allocation(&blocks[i]);
    }
    if ((result == 0) &&
        (test_verify_all_active(blocks, TEST_FRAGMENT_BLOCK_COUNT) != 0)) {
        result = 1;
    }

    /* Refill the holes with different sizes to exercise free-list splitting. */
    for (i = 1U; (i < TEST_FRAGMENT_BLOCK_COUNT) && (result == 0); i += 2U) {
        blocks[i].size = 17U + ((i * 53U) % 389U);
        blocks[i].signature = 0x30000000U + (uint32_t)i;
        blocks[i].memory = (unsigned char *)sgl_malloc(blocks[i].size);
        if (blocks[i].memory == NULL) {
            result = 1;
        }
        else {
            test_fill_allocation(&blocks[i]);
        }
    }
    if ((result == 0) &&
        (test_verify_all_active(blocks, TEST_FRAGMENT_BLOCK_COUNT) != 0)) {
        result = 1;
    }

    /* Free in a non-address order so both left and right coalescing are used. */
    for (i = 0U; i < TEST_FRAGMENT_BLOCK_COUNT; i += 3U) {
        test_release_allocation(&blocks[i]);
    }
    for (i = 2U; i < TEST_FRAGMENT_BLOCK_COUNT; i += 3U) {
        test_release_allocation(&blocks[i]);
    }
    for (i = 1U; i < TEST_FRAGMENT_BLOCK_COUNT; i += 3U) {
        test_release_allocation(&blocks[i]);
    }

    if (result == 0) {
        large.size = TEST_POOL_SIZE / 2U;
        large.signature = 0x4C415247U;
        large.memory = (unsigned char *)sgl_malloc(large.size);
        if (large.memory == NULL) {
            result = 1;
        }
        else {
            test_fill_allocation(&large);
            result = test_verify_allocation(&large);
        }
    }
    test_release_allocation(&large);

    for (i = 0U; i < TEST_FRAGMENT_BLOCK_COUNT; ++i) {
        test_release_allocation(&blocks[i]);
    }

    return result;
}

static int test_randomized_sequential_stress(void)
{
    test_allocation_t slots[TEST_SEQUENTIAL_SLOT_COUNT] = { { NULL, 0U, 0U } };
    uint32_t random_state = 0x6D2B79F5U;
    size_t iteration;
    size_t i;
    int result = 0;

    for (iteration = 0U;
         (iteration < TEST_SEQUENTIAL_ITERATIONS) && (result == 0);
         ++iteration) {
        uint32_t random_value;
        size_t slot;

        random_value = test_random_next(&random_state);
        slot = (size_t)(random_value % TEST_SEQUENTIAL_SLOT_COUNT);
        if (slots[slot].memory != NULL) {
            if (test_verify_allocation(&slots[slot]) != 0) {
                result = 1;
            }
            test_release_allocation(&slots[slot]);
        }
        else {
            slots[slot].size = 1U + (size_t)((random_value >> 8U) % 1536U);
            slots[slot].signature = random_value ^ (uint32_t)iteration;
            if ((iteration % 11U) == 0U) {
                slots[slot].memory =
                    (unsigned char *)sgl_calloc(slots[slot].size, 1U);
                if ((slots[slot].memory != NULL) &&
                    (test_verify_zero(slots[slot].memory, slots[slot].size) != 0)) {
                    result = 1;
                }
            }
            else {
                slots[slot].memory =
                    (unsigned char *)sgl_malloc(slots[slot].size);
            }

            /*
             * Allocation failure is valid under temporary pool exhaustion.
             * Reset the slot metadata so later verification treats it as empty.
             */
            if (slots[slot].memory != NULL) {
                test_fill_allocation(&slots[slot]);
            }
            else {
                slots[slot].size = 0U;
                slots[slot].signature = 0U;
            }
        }

        if (((iteration % 257U) == 0U) &&
            (test_verify_all_active(slots, TEST_SEQUENTIAL_SLOT_COUNT) != 0)) {
            result = 1;
        }
    }

    for (i = 0U; i < TEST_SEQUENTIAL_SLOT_COUNT; ++i) {
        if ((slots[i].memory != NULL) &&
            (test_verify_allocation(&slots[i]) != 0)) {
            result = 1;
        }
        test_release_allocation(&slots[i]);
    }

    return result;
}

static int test_pool_exhaustion_and_recovery(void)
{
    test_allocation_t blocks[TEST_EXHAUST_BLOCK_COUNT] = { { NULL, 0U, 0U } };
    test_allocation_t recovered = { NULL, 0U, 0U };
    size_t allocation_count = 0U;
    size_t i;
    int result = 0;

    while (allocation_count < TEST_EXHAUST_BLOCK_COUNT) {
        blocks[allocation_count].size = 113U;
        blocks[allocation_count].signature =
            0x50000000U + (uint32_t)allocation_count;
        blocks[allocation_count].memory =
            (unsigned char *)sgl_malloc(blocks[allocation_count].size);
        if (blocks[allocation_count].memory == NULL) {
            break;
        }
        test_fill_allocation(&blocks[allocation_count]);
        allocation_count++;
    }

    if ((allocation_count == 0U) ||
        (allocation_count == TEST_EXHAUST_BLOCK_COUNT)) {
        result = 1;
    }
    if ((result == 0) &&
        (test_verify_all_active(blocks, allocation_count) != 0)) {
        result = 1;
    }

    for (i = 0U; i < allocation_count; i += 2U) {
        test_release_allocation(&blocks[i]);
    }
    if ((result == 0) &&
        (test_verify_all_active(blocks, allocation_count) != 0)) {
        result = 1;
    }
    for (i = 1U; i < allocation_count; i += 2U) {
        test_release_allocation(&blocks[i]);
    }

    recovered.size = (TEST_POOL_SIZE * 3U) / 4U;
    recovered.signature = 0x5245434FU;
    recovered.memory = (unsigned char *)sgl_malloc(recovered.size);
    if (recovered.memory == NULL) {
        result = 1;
    }
    else {
        test_fill_allocation(&recovered);
        if (test_verify_allocation(&recovered) != 0) {
            result = 1;
        }
    }
    test_release_allocation(&recovered);

    for (i = 0U; i < allocation_count; ++i) {
        test_release_allocation(&blocks[i]);
    }

    return result;
}

#if defined(SGL_CFG_HAS_PTHREAD)
typedef struct {
    size_t thread_index;
    int result;
} test_thread_argument_t;

typedef struct {
    test_allocation_t *allocations;
    size_t allocation_count;
    size_t thread_index;
    int result;
} test_handoff_argument_t;

static void *test_thread_stress_routine(void *argument)
{
    test_thread_argument_t *thread_argument;
    test_allocation_t slots[TEST_THREAD_SLOT_COUNT] = { { NULL, 0U, 0U } };
    uint32_t random_state;
    size_t iteration;
    size_t i;
    int result = 0;

    thread_argument = (test_thread_argument_t *)argument;
    random_state = 0x9E3779B9U ^ (uint32_t)(thread_argument->thread_index + 1U);

    for (iteration = 0U;
         (iteration < TEST_THREAD_ITERATIONS) && (result == 0);
         ++iteration) {
        uint32_t random_value;
        size_t slot;

        random_value = test_random_next(&random_state);
        slot = (size_t)(random_value % TEST_THREAD_SLOT_COUNT);
        if (slots[slot].memory != NULL) {
            if (test_verify_allocation(&slots[slot]) != 0) {
                result = 1;
            }
            test_release_allocation(&slots[slot]);
        }
        else {
            slots[slot].size = 1U + (size_t)((random_value >> 7U) % 768U);
            slots[slot].signature =
                random_value ^ (uint32_t)thread_argument->thread_index;
            slots[slot].memory = (unsigned char *)sgl_malloc(slots[slot].size);
            if (slots[slot].memory != NULL) {
                test_fill_allocation(&slots[slot]);
            }
            else {
                slots[slot].size = 0U;
                slots[slot].signature = 0U;
            }
        }

        if (((iteration % 509U) == 0U) &&
            (test_verify_all_active(slots, TEST_THREAD_SLOT_COUNT) != 0)) {
            result = 1;
        }
    }

    for (i = 0U; i < TEST_THREAD_SLOT_COUNT; ++i) {
        if ((slots[i].memory != NULL) &&
            (test_verify_allocation(&slots[i]) != 0)) {
            result = 1;
        }
        test_release_allocation(&slots[i]);
    }
    thread_argument->result = result;

    return NULL;
}

static int test_concurrent_stress(void)
{
    pthread_t threads[TEST_THREAD_COUNT];
    test_thread_argument_t arguments[TEST_THREAD_COUNT];
    size_t created_count = 0U;
    size_t i = 0U;
    int result = 0;

    while ((i < TEST_THREAD_COUNT) && (result == 0)) {
        arguments[i].thread_index = i;
        arguments[i].result = 0;
        if (pthread_create(
                &threads[i],
                NULL,
                test_thread_stress_routine,
                &arguments[i]) != 0) {
            result = 1;
        }
        else {
            created_count++;
        }
        i++;
    }

    for (i = 0U; i < created_count; ++i) {
        if (pthread_join(threads[i], NULL) != 0) {
            result = 1;
        }
        if (arguments[i].result != 0) {
            result = 1;
        }
    }

    return result;
}

static void *test_handoff_free_routine(void *argument)
{
    test_handoff_argument_t *handoff_argument;
    size_t i;
    int result = 0;

    handoff_argument = (test_handoff_argument_t *)argument;
    for (i = handoff_argument->thread_index;
         i < handoff_argument->allocation_count;
         i += TEST_THREAD_COUNT) {
        if (test_verify_allocation(&handoff_argument->allocations[i]) != 0) {
            result = 1;
        }
        test_release_allocation(&handoff_argument->allocations[i]);
    }
    handoff_argument->result = result;

    return NULL;
}

static int test_cross_thread_handoff(void)
{
    test_allocation_t blocks[TEST_HANDOFF_BLOCK_COUNT] = { { NULL, 0U, 0U } };
    pthread_t threads[TEST_THREAD_COUNT];
    test_handoff_argument_t arguments[TEST_THREAD_COUNT];
    size_t created_count = 0U;
    size_t i = 0U;
    int result = 0;

    for (i = 0U; (i < TEST_HANDOFF_BLOCK_COUNT) && (result == 0); ++i) {
        blocks[i].size = 8U + ((i * 61U) % 521U);
        blocks[i].signature = 0x70000000U + (uint32_t)i;
        blocks[i].memory = (unsigned char *)sgl_malloc(blocks[i].size);
        if (blocks[i].memory == NULL) {
            result = 1;
        }
        else {
            test_fill_allocation(&blocks[i]);
        }
    }

    i = 0U;
    while ((i < TEST_THREAD_COUNT) && (result == 0)) {
        arguments[i].allocations = blocks;
        arguments[i].allocation_count = TEST_HANDOFF_BLOCK_COUNT;
        arguments[i].thread_index = i;
        arguments[i].result = 0;
        if (pthread_create(
                &threads[i],
                NULL,
                test_handoff_free_routine,
                &arguments[i]) != 0) {
            result = 1;
        }
        else {
            created_count++;
        }
        i++;
    }

    for (i = 0U; i < created_count; ++i) {
        if (pthread_join(threads[i], NULL) != 0) {
            result = 1;
        }
        if (arguments[i].result != 0) {
            result = 1;
        }
    }
    for (i = 0U; i < TEST_HANDOFF_BLOCK_COUNT; ++i) {
        test_release_allocation(&blocks[i]);
    }

    return result;
}
#endif

static int test_run_pool_case(const char *name, test_case_t test_case)
{
    void *pool;
    sgl_result_t deinitialize_result;
    int case_result;
    int result = 0;

    test_prepare_pool_storage();
    pool = &test_pool_storage[TEST_POOL_GUARD_SIZE + 1U];
    if (sgl_memory_pool_initialize(pool, TEST_POOL_SIZE) != SGL_SUCCESS) {
        result = 1;
    }

    case_result = 1;
    if (result == 0) {
        case_result = test_case();
        if (case_result != 0) {
            result = 1;
        }
    }

    deinitialize_result = sgl_memory_pool_deinitialize();
    if (deinitialize_result != SGL_SUCCESS) {
        /*
         * A failed deinitialization after the case is a direct leak signal:
         * at least one pool allocation was not released.
         */
        result = 1;
    }
    if (test_verify_pool_guards() != 0) {
        result = 1;
    }

    if (result == 0) {
        printf("[PASS] %s\n", name);
    }
    else {
        printf(
            "[FAIL] %s (case=%d, deinitialize=%d)\n",
            name,
            case_result,
            (int)deinitialize_result);
    }

    return result;
}

int main(void)
{
    int result = 0;

    if (test_uninitialized_pool_contract() != 0) {
        puts("[FAIL] uninitialized pool contract");
        result = 1;
    }
    else {
        puts("[PASS] uninitialized pool contract");
    }

    if (test_run_pool_case("basic contract", test_basic_contract) != 0) {
        result = 1;
    }
    if (test_run_pool_case(
            "fragmentation and coalescing",
            test_fragmentation_and_coalescing) != 0) {
        result = 1;
    }
    if (test_run_pool_case(
            "randomized sequential stress",
            test_randomized_sequential_stress) != 0) {
        result = 1;
    }
    if (test_run_pool_case(
            "pool exhaustion and recovery",
            test_pool_exhaustion_and_recovery) != 0) {
        result = 1;
    }
#if defined(SGL_CFG_HAS_PTHREAD)
    if (test_run_pool_case("concurrent stress", test_concurrent_stress) != 0) {
        result = 1;
    }
    if (test_run_pool_case(
            "cross-thread allocation handoff",
            test_cross_thread_handoff) != 0) {
        result = 1;
    }
#endif

    if (result == 0) {
        puts("memory pool stress tests passed");
    }

    return result;
}
