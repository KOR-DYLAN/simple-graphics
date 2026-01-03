#include <arm_neon.h>

int main(void)
{
    float32x4_t a = vdupq_n_f32(1.0f);
    float32x4_t b = vdupq_n_f32(2.0f);
    float32x4_t c = vaddq_f32(a, b);

    return (int)vgetq_lane_f32(c, 0);
}
