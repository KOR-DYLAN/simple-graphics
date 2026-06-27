#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sgl-core.h>
#include "util.h"

#if defined(SGL_TEST_HAS_CAIRO)
#include <cairo.h>
#endif  /* SGL_TEST_HAS_CAIRO */

#if defined(SGL_TEST_HAS_SDL2)
#include <SDL.h>
#endif  /* SGL_TEST_HAS_SDL2 */

#if defined(SGL_TEST_HAS_NE10)
#include <NE10_imgproc.h>
#endif  /* SGL_TEST_HAS_NE10 */

#define SGL_TEST_ARRAY_SIZE(array) \
    (sizeof(array) / sizeof((array)[0]))
#define SGL_TEST_REPEAT_COUNT       (10)
#define SGL_TEST_MEMORY_POOL_SIZE   (64U * 1024U * 1024U)
#define SGL_TEST_BENCHMARK_DIR      "benchmark"
#define SGL_TEST_BENCHMARK_CSV      SGL_TEST_BENCHMARK_DIR "/resize-benchmark.csv"
#define SGL_TEST_BUILD_DIR          "build"
#define SGL_TEST_OUTPUT_DIR         SGL_TEST_BUILD_DIR "/output"

/*
 * Resize test design
 * ------------------
 * The resize application is both a functional smoke test and a benchmark
 * driver.  Keep the benchmark matrix data-driven so a new case can be added
 * without copying a long block of control flow.
 *
 *      dimensions[]  x  methods[]  x  thread_counts[]
 *            |              |               |
 *            +--------------+---------------+
 *                           |
 *                     one benchmark case
 *                           |
 *          repeat N times, record min/avg/max latency and debug PNG
 *
 * Extension points:
 * - Add a new output size to sgl_test_resize_dimensions.
 * - Add a new backend or interpolation method to sgl_test_resize_methods.
 * - Add a new worker count to sgl_test_thread_counts.
 * - Optional comparison libraries are listed as ordinary methods.  CMake
 *   defines SGL_TEST_HAS_CAIRO or SGL_TEST_HAS_SDL2 only when benchmark
 *   comparison dependencies are explicitly enabled, so a normal test build
 *   does not download or build those extra backends.
 *
 * The text table is optimized for reading in a terminal.  The CSV file is
 * stable and can be diffed or imported into a spreadsheet for benchmark
 * comparison between commits, compiler options, SIMD modes, or thread counts.
 * Resized PNG files are written separately under build/output for visual
 * debugging and result inspection.
 */

typedef sgl_result_t (*sgl_test_resize_runner_t)(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp);

typedef void *(*sgl_test_lut_creator_t)(
    int32_t d_width,
    int32_t d_height,
    int32_t s_width,
    int32_t s_height);
typedef void (*sgl_test_lut_destroyer_t)(void *lut);

typedef struct {
    uint8_t *buf;
    int32_t width;
    int32_t height;
    int32_t bpp;
} sgl_test_resize_source_t;

typedef struct {
    const char *name;
    int32_t width;
    int32_t height;
} sgl_test_resize_dimension_t;

typedef struct {
    const char *name;
    const char *backend;
    sgl_test_resize_runner_t run;
    sgl_test_lut_creator_t create_lut;
    sgl_test_lut_destroyer_t destroy_lut;
    int32_t supports_threads;
} sgl_test_resize_method_t;

typedef struct {
    size_t workers;
    sgl_threadpool_t *pool;
} sgl_test_thread_context_t;

typedef struct {
    uint64_t min_us;
    uint64_t max_us;
    uint64_t total_us;
} sgl_test_benchmark_stats_t;

typedef struct {
    const sgl_test_resize_dimension_t *dimension;
    const sgl_test_resize_method_t *method;
    sgl_test_thread_context_t *thread;
} sgl_test_resize_case_t;

static unsigned char sgl_test_memory_pool[SGL_TEST_MEMORY_POOL_SIZE];

static sgl_result_t sgl_test_resize_nearest(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp);
static sgl_result_t sgl_test_resize_bilinear(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp);
static sgl_result_t sgl_test_resize_bicubic(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp);
#if defined(SGL_TEST_HAS_CAIRO)
static sgl_result_t sgl_test_resize_cairo_nearest(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp);
static sgl_result_t sgl_test_resize_cairo_bilinear(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp);
#endif  /* SGL_TEST_HAS_CAIRO */
#if defined(SGL_TEST_HAS_SDL2)
static sgl_result_t sgl_test_resize_sdl2_nearest(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp);
static sgl_result_t sgl_test_resize_sdl2_bilinear(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp);
#endif  /* SGL_TEST_HAS_SDL2 */
#if defined(SGL_TEST_HAS_NE10)
static int sgl_test_ne10_init(void);
static sgl_result_t sgl_test_resize_ne10_bilinear(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp);
#endif  /* SGL_TEST_HAS_NE10 */
#if defined(SGL_CFG_HAS_SIMD)
static sgl_result_t sgl_test_resize_nearest_simd(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp);
static sgl_result_t sgl_test_resize_bilinear_simd(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp);
static sgl_result_t sgl_test_resize_bicubic_simd(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp);
#endif  /* SGL_CFG_HAS_SIMD */

static void *sgl_test_create_nearest_lut(int32_t d_width,
                                         int32_t d_height,
                                         int32_t s_width,
                                         int32_t s_height);
static void *sgl_test_create_bilinear_lut(int32_t d_width,
                                          int32_t d_height,
                                          int32_t s_width,
                                          int32_t s_height);
static void *sgl_test_create_bicubic_lut(int32_t d_width,
                                         int32_t d_height,
                                         int32_t s_width,
                                         int32_t s_height);
static void sgl_test_destroy_nearest_lut(void *lut);
static void sgl_test_destroy_bilinear_lut(void *lut);
static void sgl_test_destroy_bicubic_lut(void *lut);
static int sgl_test_thread_contexts_init(sgl_test_thread_context_t *threads,
                                         size_t count);
static void sgl_test_thread_contexts_deinit(sgl_test_thread_context_t *threads,
                                            size_t count);
static int sgl_test_run_resize_matrix(sgl_test_resize_source_t *src);
static int sgl_test_run_resize_case(const sgl_test_resize_case_t *test_case,
                                    sgl_test_resize_source_t *src,
                                    FILE *csv,
                                    size_t case_index);
static int sgl_test_should_run_resize_case(
    const sgl_test_resize_case_t *test_case);
static int sgl_test_make_benchmark_dir(void);
static int sgl_test_make_output_dir(void);
static int sgl_test_make_dir(const char *path);
static void sgl_test_stats_init(sgl_test_benchmark_stats_t *stats);
static void sgl_test_stats_add_sample(sgl_test_benchmark_stats_t *stats,
                                      uint64_t elapsed_us);
static uint64_t sgl_test_stats_average_us(
    const sgl_test_benchmark_stats_t *stats);
static void sgl_test_print_table_header(void);
static void sgl_test_write_csv_header(FILE *csv);
static int sgl_test_write_png_output(const sgl_test_resize_case_t *test_case,
                                     sgl_test_resize_source_t *src,
                                     uint8_t *dst,
                                     size_t case_index,
                                     char *path,
                                     size_t path_size);

static const sgl_test_resize_dimension_t sgl_test_resize_dimensions[] = {
    { .name = "vga",  .width =  640, .height =  480 },
    { .name = "hd",   .width = 1280, .height =  720 },
    { .name = "fhd",  .width = 1920, .height = 1080 },
    { .name = "qhd",  .width = 2560, .height = 1440 },
};

static const sgl_test_resize_method_t sgl_test_resize_methods[] = {
    { .name = "nearest", .backend = "generic",
      .run = sgl_test_resize_nearest,
      .create_lut = NULL, .destroy_lut = NULL,
      .supports_threads = 1 },
    { .name = "bilinear", .backend = "generic",
      .run = sgl_test_resize_bilinear,
      .create_lut = NULL, .destroy_lut = NULL,
      .supports_threads = 1 },
    { .name = "bicubic", .backend = "generic",
      .run = sgl_test_resize_bicubic,
      .create_lut = NULL, .destroy_lut = NULL,
      .supports_threads = 1 },
    { .name = "nearest", .backend = "generic-lut",
      .run = sgl_test_resize_nearest,
      .create_lut = sgl_test_create_nearest_lut,
      .destroy_lut = sgl_test_destroy_nearest_lut,
      .supports_threads = 1 },
    { .name = "bilinear", .backend = "generic-lut",
      .run = sgl_test_resize_bilinear,
      .create_lut = sgl_test_create_bilinear_lut,
      .destroy_lut = sgl_test_destroy_bilinear_lut,
      .supports_threads = 1 },
    { .name = "bicubic", .backend = "generic-lut",
      .run = sgl_test_resize_bicubic,
      .create_lut = sgl_test_create_bicubic_lut,
      .destroy_lut = sgl_test_destroy_bicubic_lut,
      .supports_threads = 1 },
#if defined(SGL_TEST_HAS_CAIRO)
    { .name = "nearest", .backend = "cairo",
      .run = sgl_test_resize_cairo_nearest,
      .create_lut = NULL, .destroy_lut = NULL,
      .supports_threads = 0 },
    { .name = "bilinear", .backend = "cairo",
      .run = sgl_test_resize_cairo_bilinear,
      .create_lut = NULL, .destroy_lut = NULL,
      .supports_threads = 0 },
#endif  /* SGL_TEST_HAS_CAIRO */
#if defined(SGL_TEST_HAS_SDL2)
    { .name = "nearest", .backend = "sdl2",
      .run = sgl_test_resize_sdl2_nearest,
      .create_lut = NULL, .destroy_lut = NULL,
      .supports_threads = 0 },
    { .name = "bilinear", .backend = "sdl2",
      .run = sgl_test_resize_sdl2_bilinear,
      .create_lut = NULL, .destroy_lut = NULL,
      .supports_threads = 0 },
#endif  /* SGL_TEST_HAS_SDL2 */
#if defined(SGL_TEST_HAS_NE10)
    { .name = "bilinear", .backend = "ne10",
      .run = sgl_test_resize_ne10_bilinear,
      .create_lut = NULL, .destroy_lut = NULL,
      .supports_threads = 0 },
#endif  /* SGL_TEST_HAS_NE10 */
#if defined(SGL_CFG_HAS_SIMD)
    { .name = "nearest", .backend = "simd",
      .run = sgl_test_resize_nearest_simd,
      .create_lut = NULL, .destroy_lut = NULL,
      .supports_threads = 1 },
    { .name = "bilinear", .backend = "simd",
      .run = sgl_test_resize_bilinear_simd,
      .create_lut = NULL, .destroy_lut = NULL,
      .supports_threads = 1 },
    { .name = "bicubic", .backend = "simd",
      .run = sgl_test_resize_bicubic_simd,
      .create_lut = NULL, .destroy_lut = NULL,
      .supports_threads = 1 },
    { .name = "nearest", .backend = "simd-lut",
      .run = sgl_test_resize_nearest_simd,
      .create_lut = sgl_test_create_nearest_lut,
      .destroy_lut = sgl_test_destroy_nearest_lut,
      .supports_threads = 1 },
    { .name = "bilinear", .backend = "simd-lut",
      .run = sgl_test_resize_bilinear_simd,
      .create_lut = sgl_test_create_bilinear_lut,
      .destroy_lut = sgl_test_destroy_bilinear_lut,
      .supports_threads = 1 },
    { .name = "bicubic", .backend = "simd-lut",
      .run = sgl_test_resize_bicubic_simd,
      .create_lut = sgl_test_create_bicubic_lut,
      .destroy_lut = sgl_test_destroy_bicubic_lut,
      .supports_threads = 1 },
#endif  /* SGL_CFG_HAS_SIMD */
};

static sgl_test_thread_context_t sgl_test_thread_counts[] = {
    { .workers = 1U, .pool = NULL },
#if defined(SGL_CFG_HAS_THREAD)
    { .workers = 2U, .pool = NULL },
    { .workers = 4U, .pool = NULL },
    { .workers = 8U, .pool = NULL },
#endif  /* SGL_CFG_HAS_THREAD */
};

int main(int argc, char *argv[])
{
    sgl_test_png_t *png = NULL;
    sgl_test_resize_source_t src;
    int result = 0;

    if (argc < 2) {
        (void)fprintf(stderr, "usage: %s <input.png>\n", argv[0]);
        result = 1;
    }

    if ((result == 0) &&
        (sgl_memory_pool_initialize(
            sgl_test_memory_pool,
            sizeof(sgl_test_memory_pool)) != SGL_SUCCESS)) {
        result = 1;
    }

#if defined(SGL_TEST_HAS_NE10)
    if ((result == 0) && (sgl_test_ne10_init() != 0)) {
        result = 1;
    }
#endif  /* SGL_TEST_HAS_NE10 */

    if (result == 0) {
        png = sgl_test_load_png(argv[1]);
        if (png != NULL) {
            src.buf = png->data;
            src.width = png->width;
            src.height = png->height;
            src.bpp = png->channels;

            result = sgl_test_run_resize_matrix(&src);

            sgl_test_release_png(png);
        }
        else {
            result = 1;
        }
    }

    if (sgl_memory_pool_deinitialize() != SGL_SUCCESS) {
        result = 1;
    }

    return result;
}

static sgl_result_t sgl_test_resize_nearest(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp)
{
    sgl_result_t result;

    result = sgl_generic_resize_nearest(
        pool,
        (sgl_nearest_neighbor_lookup_t *)lut,
        dst, d_width, d_height,
        src, s_width, s_height, bpp);

    return result;
}

static sgl_result_t sgl_test_resize_bilinear(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp)
{
    sgl_result_t result;

    result = sgl_generic_resize_bilinear(
        pool,
        (sgl_bilinear_lookup_t *)lut,
        dst, d_width, d_height,
        src, s_width, s_height, bpp);

    return result;
}

static sgl_result_t sgl_test_resize_bicubic(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp)
{
    sgl_result_t result;

    result = sgl_generic_resize_bicubic(
        pool,
        (sgl_bicubic_lookup_t *)lut,
        dst, d_width, d_height,
        src, s_width, s_height, bpp);

    return result;
}

#if defined(SGL_TEST_HAS_CAIRO)
static sgl_result_t sgl_test_resize_cairo(
    cairo_filter_t filter,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp)
{
    cairo_surface_t *src_surface = NULL;
    cairo_surface_t *dst_surface = NULL;
    cairo_t *context = NULL;
    cairo_pattern_t *pattern = NULL;
    double scale_x;
    double scale_y;
    sgl_result_t result = SGL_SUCCESS;

    /*
     * Cairo exposes nearest/bilinear filtering through image surfaces.  Keep
     * source and destination as already-loaded 32-bit test buffers so the timed
     * region measures the resize API path instead of an extra format conversion
     * stage.  Cairo's native 32-bit channel order is not used as a
     * pixel-accuracy oracle here.
     */
    if (bpp != SGL_BPP32) {
        result = SGL_ERROR_NOT_SUPPORTED;
    }

    if (result == SGL_SUCCESS) {
        src_surface = cairo_image_surface_create_for_data(
            src,
            CAIRO_FORMAT_ARGB32,
            s_width,
            s_height,
            s_width * bpp);
        dst_surface = cairo_image_surface_create_for_data(
            dst,
            CAIRO_FORMAT_ARGB32,
            d_width,
            d_height,
            d_width * bpp);
        if ((cairo_surface_status(src_surface) != CAIRO_STATUS_SUCCESS) ||
            (cairo_surface_status(dst_surface) != CAIRO_STATUS_SUCCESS)) {
            result = SGL_FAILURE;
        }
    }

    if (result == SGL_SUCCESS) {
        context = cairo_create(dst_surface);
        if (cairo_status(context) != CAIRO_STATUS_SUCCESS) {
            result = SGL_FAILURE;
        }
    }

    if (result == SGL_SUCCESS) {
        scale_x = (double)d_width / (double)s_width;
        scale_y = (double)d_height / (double)s_height;
        cairo_scale(context, scale_x, scale_y);
        cairo_set_source_surface(context, src_surface, 0.0, 0.0);
        pattern = cairo_get_source(context);
        cairo_pattern_set_filter(pattern, filter);
        cairo_paint(context);
        cairo_surface_flush(dst_surface);

        if (cairo_status(context) != CAIRO_STATUS_SUCCESS) {
            result = SGL_FAILURE;
        }
    }

    if (context != NULL) {
        cairo_destroy(context);
    }
    if (src_surface != NULL) {
        cairo_surface_destroy(src_surface);
    }
    if (dst_surface != NULL) {
        cairo_surface_destroy(dst_surface);
    }

    return result;
}

static sgl_result_t sgl_test_resize_cairo_nearest(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp)
{
    sgl_result_t result;

    SGL_UNUSED_PARAM(pool);
    SGL_UNUSED_PARAM(lut);
    result = sgl_test_resize_cairo(CAIRO_FILTER_NEAREST, dst, d_width,
                                   d_height, src, s_width, s_height, bpp);

    return result;
}

static sgl_result_t sgl_test_resize_cairo_bilinear(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp)
{
    sgl_result_t result;

    SGL_UNUSED_PARAM(pool);
    SGL_UNUSED_PARAM(lut);
    result = sgl_test_resize_cairo(CAIRO_FILTER_BILINEAR, dst, d_width,
                                   d_height, src, s_width, s_height, bpp);

    return result;
}
#endif  /* SGL_TEST_HAS_CAIRO */

#if defined(SGL_TEST_HAS_SDL2)
static sgl_result_t sgl_test_resize_sdl2(
    int scale_mode,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp)
{
    SDL_Surface *src_surface = NULL;
    SDL_Surface *dst_surface = NULL;
    sgl_result_t result = SGL_SUCCESS;

    /*
     * SDL_BlitScaled is benchmarked through software surfaces backed directly
     * by the same source/destination buffers used by SGL.  Setting the hint per
     * call keeps nearest and bilinear rows explicit in the CSV.
     */
    if (bpp != SGL_BPP32) {
        result = SGL_ERROR_NOT_SUPPORTED;
    }

    if (result == SGL_SUCCESS) {
        if (SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
                        (scale_mode == 0) ? "0" : "1") != SDL_TRUE) {
            result = SGL_FAILURE;
        }
    }

    if (result == SGL_SUCCESS) {
        src_surface = SDL_CreateRGBSurfaceWithFormatFrom(
            src,
            s_width,
            s_height,
            bpp * 8,
            s_width * bpp,
            SDL_PIXELFORMAT_RGBA32);
        dst_surface = SDL_CreateRGBSurfaceWithFormatFrom(
            dst,
            d_width,
            d_height,
            bpp * 8,
            d_width * bpp,
            SDL_PIXELFORMAT_RGBA32);
        if ((src_surface == NULL) || (dst_surface == NULL)) {
            result = SGL_FAILURE;
        }
    }

    if (result == SGL_SUCCESS) {
        if (SDL_BlitScaled(src_surface, NULL, dst_surface, NULL) != 0) {
            result = SGL_FAILURE;
        }
    }

    if (src_surface != NULL) {
        SDL_FreeSurface(src_surface);
    }
    if (dst_surface != NULL) {
        SDL_FreeSurface(dst_surface);
    }

    return result;
}

static sgl_result_t sgl_test_resize_sdl2_nearest(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp)
{
    sgl_result_t result;

    SGL_UNUSED_PARAM(pool);
    SGL_UNUSED_PARAM(lut);
    result = sgl_test_resize_sdl2(0, dst, d_width, d_height, src, s_width,
                                  s_height, bpp);

    return result;
}

static sgl_result_t sgl_test_resize_sdl2_bilinear(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp)
{
    sgl_result_t result;

    SGL_UNUSED_PARAM(pool);
    SGL_UNUSED_PARAM(lut);
    result = sgl_test_resize_sdl2(1, dst, d_width, d_height, src, s_width,
                                  s_height, bpp);

    return result;
}
#endif  /* SGL_TEST_HAS_SDL2 */

#if defined(SGL_TEST_HAS_NE10)
static int sgl_test_ne10_init(void)
{
    int result = 0;

    /*
     * NE10 exposes image resize through a function pointer selected by
     * ne10_init_imgproc().  On aarch64, NEON/ASIMD is part of the platform
     * baseline, so the optimized path is the intended comparison target.
     */
    if (ne10_init_imgproc(NE10_OK) != NE10_OK) {
        result = 1;
    }

    return result;
}

static sgl_result_t sgl_test_resize_ne10_bilinear(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp)
{
    sgl_result_t result = SGL_SUCCESS;

    SGL_UNUSED_PARAM(pool);
    SGL_UNUSED_PARAM(lut);
    if (bpp != SGL_BPP32) {
        result = SGL_ERROR_NOT_SUPPORTED;
    }

    if (result == SGL_SUCCESS) {
        ne10_img_resize_bilinear_rgba(
            (ne10_uint8_t *)dst,
            (ne10_uint32_t)d_width,
            (ne10_uint32_t)d_height,
            (ne10_uint8_t *)src,
            (ne10_uint32_t)s_width,
            (ne10_uint32_t)s_height,
            (ne10_uint32_t)(s_width * bpp));
    }

    return result;
}
#endif  /* SGL_TEST_HAS_NE10 */

#if defined(SGL_CFG_HAS_SIMD)
static sgl_result_t sgl_test_resize_nearest_simd(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp)
{
    sgl_result_t result;

    result = sgl_simd_resize_nearest(
        pool,
        (sgl_nearest_neighbor_lookup_t *)lut,
        dst, d_width, d_height,
        src, s_width, s_height, bpp);

    return result;
}

static sgl_result_t sgl_test_resize_bilinear_simd(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp)
{
    sgl_result_t result;

    result = sgl_simd_resize_bilinear(
        pool,
        (sgl_bilinear_lookup_t *)lut,
        dst, d_width, d_height,
        src, s_width, s_height, bpp);

    return result;
}

static sgl_result_t sgl_test_resize_bicubic_simd(
    sgl_threadpool_t *pool,
    void *lut,
    uint8_t *dst,
    int32_t d_width,
    int32_t d_height,
    uint8_t *src,
    int32_t s_width,
    int32_t s_height,
    int32_t bpp)
{
    sgl_result_t result;

    result = sgl_simd_resize_bicubic(
        pool,
        (sgl_bicubic_lookup_t *)lut,
        dst, d_width, d_height,
        src, s_width, s_height, bpp);

    return result;
}
#endif  /* SGL_CFG_HAS_SIMD */

static void *sgl_test_create_nearest_lut(int32_t d_width,
                                         int32_t d_height,
                                         int32_t s_width,
                                         int32_t s_height)
{
    void *lut;

    lut = (void *)sgl_generic_create_nearest_neighbor_lut(
        d_width, d_height, s_width, s_height);

    return lut;
}

static void *sgl_test_create_bilinear_lut(int32_t d_width,
                                          int32_t d_height,
                                          int32_t s_width,
                                          int32_t s_height)
{
    void *lut;

    lut = (void *)sgl_generic_create_bilinear_lut(
        d_width, d_height, s_width, s_height);

    return lut;
}

static void *sgl_test_create_bicubic_lut(int32_t d_width,
                                         int32_t d_height,
                                         int32_t s_width,
                                         int32_t s_height)
{
    void *lut;

    lut = (void *)sgl_generic_create_bicubic_lut(
        d_width, d_height, s_width, s_height);

    return lut;
}

static void sgl_test_destroy_nearest_lut(void *lut)
{
    sgl_generic_destroy_nearest_neighbor_lut(
        (sgl_nearest_neighbor_lookup_t *)lut);
}

static void sgl_test_destroy_bilinear_lut(void *lut)
{
    sgl_generic_destroy_bilinear_lut((sgl_bilinear_lookup_t *)lut);
}

static void sgl_test_destroy_bicubic_lut(void *lut)
{
    sgl_generic_destroy_bicubic_lut((sgl_bicubic_lookup_t *)lut);
}

static int sgl_test_thread_contexts_init(sgl_test_thread_context_t *threads,
                                         size_t count)
{
    size_t index;
    int result = 0;

    for (index = 0U; index < count; ++index) {
        /*
         * A one-worker case intentionally uses a NULL pool.  That measures the
         * single-thread resize path without queue or worker-thread overhead.
         */
        if (threads[index].workers > 1U) {
#if defined(SGL_CFG_HAS_THREAD)
            threads[index].pool = sgl_threadpool_create(
                threads[index].workers,
                SGL_THREADPOOL_DEFAULT_MAX_ROUTINE_LISTS,
                "resize_pool");
            if (threads[index].pool == NULL) {
                result = 1;
            }
#else
            result = 1;
#endif  /* SGL_CFG_HAS_THREAD */
        }
    }

    return result;
}

static void sgl_test_thread_contexts_deinit(sgl_test_thread_context_t *threads,
                                            size_t count)
{
    size_t index;

    for (index = 0U; index < count; ++index) {
        if (threads[index].pool != NULL) {
#if defined(SGL_CFG_HAS_THREAD)
            (void)sgl_threadpool_destroy(threads[index].pool);
#endif  /* SGL_CFG_HAS_THREAD */
            threads[index].pool = NULL;
        }
    }
}

static int sgl_test_run_resize_matrix(sgl_test_resize_source_t *src)
{
    FILE *csv = NULL;
    sgl_test_resize_case_t test_case;
    size_t dimension_index;
    size_t method_index;
    size_t thread_index;
    size_t case_index = 0U;
    int result = 0;

    if (sgl_test_thread_contexts_init(
            sgl_test_thread_counts,
            SGL_TEST_ARRAY_SIZE(sgl_test_thread_counts)) != 0) {
        result = 1;
    }

    if (result == 0) {
        result = sgl_test_make_benchmark_dir();
    }

    if (result == 0) {
        result = sgl_test_make_output_dir();
    }

    if (result == 0) {
        csv = fopen(SGL_TEST_BENCHMARK_CSV, "w");
        if (csv == NULL) {
            result = 1;
        }
    }

    if (result == 0) {
        sgl_test_write_csv_header(csv);
        sgl_test_print_table_header();

        for (dimension_index = 0U;
             dimension_index < SGL_TEST_ARRAY_SIZE(sgl_test_resize_dimensions);
             ++dimension_index) {
            for (method_index = 0U;
                 method_index < SGL_TEST_ARRAY_SIZE(sgl_test_resize_methods);
                 ++method_index) {
                for (thread_index = 0U;
                     thread_index < SGL_TEST_ARRAY_SIZE(sgl_test_thread_counts);
                     ++thread_index) {
                    test_case.dimension =
                        &sgl_test_resize_dimensions[dimension_index];
                    test_case.method = &sgl_test_resize_methods[method_index];
                    test_case.thread = &sgl_test_thread_counts[thread_index];

                    if (sgl_test_should_run_resize_case(&test_case) != 0) {
                        if (sgl_test_run_resize_case(
                                &test_case, src, csv, case_index) != 0) {
                            result = 1;
                        }
                        case_index++;
                    }
                }
            }
        }
    }

    if (csv != NULL) {
        (void)fclose(csv);
    }
    sgl_test_thread_contexts_deinit(
        sgl_test_thread_counts,
        SGL_TEST_ARRAY_SIZE(sgl_test_thread_counts));

    if (result == 0) {
        (void)printf("\nbenchmark csv: %s\n", SGL_TEST_BENCHMARK_CSV);
    }

    return result;
}

static int sgl_test_run_resize_case(const sgl_test_resize_case_t *test_case,
                                    sgl_test_resize_source_t *src,
                                    FILE *csv,
                                    size_t case_index)
{
    sgl_test_benchmark_stats_t stats;
    uint8_t *dst = NULL;
    uint64_t start_us;
    uint64_t elapsed_us;
    size_t repeat;
    size_t image_size;
    void *lut = NULL;
    char output_path[FILENAME_MAX];
    sgl_result_t resize_result = SGL_SUCCESS;
    int result = 0;

    image_size = (size_t)test_case->dimension->width *
                 (size_t)test_case->dimension->height *
                 (size_t)src->bpp;
    dst = (uint8_t *)sgl_calloc(1U, image_size);
    if (dst == NULL) {
        result = 1;
    }

    /*
     * Methods with create_lut measure the steady-state resize path where the
     * caller already knows the source/destination geometry and reuses the
     * precomputed coordinate table across frames.  The one-time LUT creation
     * cost is intentionally outside the repeat loop; plain generic/simd rows
     * keep measuring the convenience path that builds a temporary LUT per call.
     */
    if ((result == 0) && (test_case->method->create_lut != NULL)) {
        lut = test_case->method->create_lut(
            test_case->dimension->width,
            test_case->dimension->height,
            src->width,
            src->height);
        if (lut == NULL) {
            result = 1;
        }
    }

    sgl_test_stats_init(&stats);
    for (repeat = 0U; (result == 0) && (repeat < SGL_TEST_REPEAT_COUNT);
         ++repeat) {
        start_us = sgl_test_get_timestamp_us(0ULL);
        resize_result = test_case->method->run(
            test_case->thread->pool,
            lut,
            dst,
            test_case->dimension->width,
            test_case->dimension->height,
            src->buf,
            src->width,
            src->height,
            src->bpp);
        elapsed_us = sgl_test_get_timestamp_us(start_us);

        if (resize_result == SGL_SUCCESS) {
            sgl_test_stats_add_sample(&stats, elapsed_us);
        }
        else {
            result = 1;
        }
    }

    if (result == 0) {
        result = sgl_test_write_png_output(test_case, src, dst, case_index,
                                           output_path, sizeof(output_path));
    }

    if (result == 0) {
        (void)printf("%4zu  %-8s  %-11s  %2zu  %5dx%-5d  "
                     "%8llu.%03llums  %8llu.%03llums  "
                     "%8llu.%03llums  %s\n",
                     case_index,
                     test_case->method->name,
                     test_case->method->backend,
                     test_case->thread->workers,
                     test_case->dimension->width,
                     test_case->dimension->height,
                     sgl_test_stats_average_us(&stats) / 1000ULL,
                     sgl_test_stats_average_us(&stats) % 1000ULL,
                     stats.min_us / 1000ULL,
                     stats.min_us % 1000ULL,
                     stats.max_us / 1000ULL,
                     stats.max_us % 1000ULL,
                     output_path);

        (void)fprintf(csv,
                      "%zu,%s,%s,%zu,%d,%d,%llu,%llu,%llu\n",
                      case_index,
                      test_case->method->name,
                      test_case->method->backend,
                      test_case->thread->workers,
                      test_case->dimension->width,
                      test_case->dimension->height,
                      (unsigned long long)sgl_test_stats_average_us(&stats),
                      (unsigned long long)stats.min_us,
                      (unsigned long long)stats.max_us);
    }

    if (dst != NULL) {
        sgl_free(dst);
    }
    if ((lut != NULL) && (test_case->method->destroy_lut != NULL)) {
        test_case->method->destroy_lut(lut);
    }

    return result;
}

static int sgl_test_should_run_resize_case(
    const sgl_test_resize_case_t *test_case)
{
    int result = 1;

    /*
     * SGL generic/SIMD resize accepts an SGL threadpool and can expose
     * meaningful thread scaling.  External comparison backends such as Cairo
     * and SDL2 are called once per benchmark sample and do not consume the SGL
     * threadpool, so only keep their single-thread baseline rows.
     */
    if ((test_case->method->supports_threads == 0) &&
        (test_case->thread->workers > 1U)) {
        result = 0;
    }

    return result;
}

static int sgl_test_make_benchmark_dir(void)
{
    int result;

    result = sgl_test_make_dir(SGL_TEST_BENCHMARK_DIR);

    return result;
}

static int sgl_test_make_output_dir(void)
{
    int result;

    result = sgl_test_make_dir(SGL_TEST_BUILD_DIR);
    if (result == 0) {
        result = sgl_test_make_dir(SGL_TEST_OUTPUT_DIR);
    }

    return result;
}

static int sgl_test_make_dir(const char *path)
{
    int result = 0;

    if (mkdir(path, 0775) != 0) {
        if (errno != EEXIST) {
            result = 1;
        }
    }

    return result;
}

static void sgl_test_stats_init(sgl_test_benchmark_stats_t *stats)
{
    stats->min_us = UINT64_MAX;
    stats->max_us = 0ULL;
    stats->total_us = 0ULL;
}

static void sgl_test_stats_add_sample(sgl_test_benchmark_stats_t *stats,
                                      uint64_t elapsed_us)
{
    stats->total_us += elapsed_us;

    if (elapsed_us < stats->min_us) {
        stats->min_us = elapsed_us;
    }
    if (elapsed_us > stats->max_us) {
        stats->max_us = elapsed_us;
    }
}

static uint64_t sgl_test_stats_average_us(
    const sgl_test_benchmark_stats_t *stats)
{
    uint64_t average_us;

    average_us = stats->total_us / (uint64_t)SGL_TEST_REPEAT_COUNT;

    return average_us;
}

static void sgl_test_print_table_header(void)
{
    (void)printf("\nResize benchmark\n");
    (void)printf("repeat count: %u\n", SGL_TEST_REPEAT_COUNT);
    (void)printf("case  method    backend      th  size         "
                 "avg           min           max           output\n");
    (void)printf("----  --------  -----------  --  -----------  "
                 "------------  ------------  ------------  ------\n");
}

static void sgl_test_write_csv_header(FILE *csv)
{
    (void)fprintf(csv,
                  "case,method,backend,threads,width,height,"
                  "avg_us,min_us,max_us\n");
}

static int sgl_test_write_png_output(const sgl_test_resize_case_t *test_case,
                                     sgl_test_resize_source_t *src,
                                     uint8_t *dst,
                                     size_t case_index,
                                     char *path,
                                     size_t path_size)
{
    sgl_test_png_t resize_png;
    int written;
    int result = 0;

    written = snprintf(path,
                       path_size,
                       SGL_TEST_OUTPUT_DIR
                       "/%03zu_resize_%s_%s_%zuthread_%dx%d.png",
                       case_index,
                       test_case->method->backend,
                       test_case->method->name,
                       test_case->thread->workers,
                       test_case->dimension->width,
                       test_case->dimension->height);
    if ((written < 0) || ((size_t)written >= path_size)) {
        result = 1;
    }

    if (result == 0) {
        resize_png.data = dst;
        resize_png.width = test_case->dimension->width;
        resize_png.height = test_case->dimension->height;
        resize_png.channels = src->bpp;

        if (sgl_test_save_png(&resize_png, path) != 0) {
            result = 1;
        }
    }

    return result;
}
