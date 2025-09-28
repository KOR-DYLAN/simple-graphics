#ifndef UTIL_H_
#define UTIL_H_

typedef struct {
    uint8_t *data;
    int32_t width;
    int32_t height;
    int32_t channels;
} sgl_test_png_t;

sgl_test_png_t *sgl_test_load_png(const char *path);
int32_t sgl_test_save_png(sgl_test_png_t *png, const char *path);
void sgl_test_release_png(sgl_test_png_t *png);
int32_t sgl_test_save_data(const char *path, uint8_t *data, int32_t datasize);

uint64_t sgl_test_get_timestamp_us(uint64_t start_us);

#endif  /* !UTIL_H_ */
