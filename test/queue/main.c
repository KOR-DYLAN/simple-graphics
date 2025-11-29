#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "sgl.h"

#define ARRAY_SIZE(arr) sizeof(arr) / sizeof(arr[0])

const char *cheat_string[] = {
    "show me the money",
    "black sheep wall",
    "operation cwal",
    "power overwhelming"
};

int main(int argc, char *argv[]) {
    sgl_queue_t *queue;
    size_t capacity = ARRAY_SIZE(cheat_string);
    const char *data;
    size_t i;

    SGL_UNUSED_PARAM(argc);
    SGL_UNUSED_PARAM(argv);

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
    }
    printf("----------End Queue Test-------------\n");

    return 0;
}
