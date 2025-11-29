#include <stdio.h>
#include <stdint.h>
#include "sgl.h"
#include "util.h"

int main(int argc, char *argv[]) {
    sgl_test_png_t *png = NULL;

    SGL_UNUSED_PARAM(argc);

    png = sgl_test_load_png(argv[1]);
    if (png != NULL) {
        sgl_test_save_data("build/image.raw", png->data, png->width * png->height * png->channels);

        sgl_test_save_png(png, "build/clone.png");
        sgl_test_release_png(png);
    }

    return 0;
}
