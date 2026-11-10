#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Low-level PNG sample conversion only. No gamma, compositing, premultiplication,
 * GL upload, or C++ object is involved in the libpng setjmp/longjmp region. */
struct Olex2PngResult {
  uint32_t width, height;
  int source_bit_depth, source_color_type, source_interlace;
  int status; /* 0 decoded, 1 original texture dimensions unsupported, 2 error */
  unsigned char *rgb;
  size_t rgb_bytes, row_bytes;
  size_t allocations, releases, live_allocations, peak_bytes, live_bytes;
  unsigned warnings;
  char error[256];
};
void olex2_png_decode_memory(const unsigned char *bytes, size_t length,
                           uint32_t maximum_dimension, struct Olex2PngResult *out);
void olex2_png_result_release(struct Olex2PngResult *result);
const char *olex2_png_decoder_version(void);
#ifdef __cplusplus
}
#endif
