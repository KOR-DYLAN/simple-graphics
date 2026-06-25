#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <sgl-core.h>

#define ARRAY_SIZE(arr) sizeof(arr) / sizeof(arr[0])
#define SGL_TEST_MEMORY_POOL_SIZE   (64U * 1024U)

static unsigned char sgl_test_memory_pool[SGL_TEST_MEMORY_POOL_SIZE];

const char *cheat_string[] = {
    "show me the money",
    "black sheep wall",
    "operation cwal",
    "power overwhelming"
};

int main(int argc, char *argv[]) {
    sgl_queue_t *queue = NULL;
    size_t capacity = ARRAY_SIZE(cheat_string);
    const char *data;
    size_t i;
    int result = 0;

    SGL_UNUSED_PARAM(argc);
    SGL_UNUSED_PARAM(argv);

    if (sgl_memory_pool_initialize(
            sgl_test_memory_pool,
            sizeof(sgl_test_memory_pool)) != SGL_SUCCESS) {
        result = 1;
    }
    if (result == 0) {
        printf("----------Begin Queue Test-----------\n");
        queue = sgl_queue_create(capacity);
        if (queue != NULL) {
            for (i = 0; i < capacity; ++i) {
                sgl_queue_enqueue(queue, (const void *)cheat_string[i]);
            }

            while (true) {
                data = (const char *)sgl_queue_dequeue(queue);
                if (data == NULL) {
                    break;
                }
                puts(data);
            }
            sgl_queue_destroy(&queue);
        }
        else {
            result = 1;
        }
        printf("----------End Queue Test-------------\n");
    }
    if (sgl_memory_pool_deinitialize() != SGL_SUCCESS) {
        result = 1;
    }

    return result;
}
