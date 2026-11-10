#include "png_texture_decoder.h"
#include <png.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

/* Keep all state changed after setjmp on the heap; do not read clobbered C
 * automatic variables on the error path. The caller's output is not a png owner. */
struct DecodeState {
  const unsigned char *input;
  size_t length, offset;
  struct Olex2PngResult *out;
  png_structp png;
  png_infop info;
  png_bytep *rows;
};
union AllocationHeader { max_align_t alignment; size_t bytes; };
static png_voidp decoder_alloc(png_structp png, png_alloc_size_t size) {
  struct DecodeState *state=(struct DecodeState*)png_get_mem_ptr(png);
  union AllocationHeader *header;
  if(size>SIZE_MAX-sizeof(*header))return NULL;
  header=(union AllocationHeader*)malloc(sizeof(*header)+(size_t)size);
  if(!header)return NULL;
  header->bytes=(size_t)size;
  ++state->out->allocations;++state->out->live_allocations;
  state->out->live_bytes+=(size_t)size;
  if(state->out->live_bytes>state->out->peak_bytes)state->out->peak_bytes=state->out->live_bytes;
  return header+1;
}
static void decoder_free(png_structp png, png_voidp pointer) {
  struct DecodeState *state;
  union AllocationHeader *header;
  if(!pointer)return;
  state=(struct DecodeState*)png_get_mem_ptr(png);
  header=((union AllocationHeader*)pointer)-1;
  ++state->out->releases;--state->out->live_allocations;
  state->out->live_bytes-=header->bytes;free(header);
}
static void decoder_error(png_structp png,png_const_charp message) {
  struct DecodeState *state=(struct DecodeState*)png_get_error_ptr(png);
  snprintf(state->out->error,sizeof(state->out->error),"%s",message?message:"PNG decode error");
  png_longjmp(png,1);
}
static void decoder_warning(png_structp png,png_const_charp message) {
  struct DecodeState *state=(struct DecodeState*)png_get_error_ptr(png);
  (void)message;++state->out->warnings;
}
static void decoder_read(png_structp png,png_bytep buffer,png_size_t length) {
  struct DecodeState *state=(struct DecodeState*)png_get_io_ptr(png);
  if(state->offset>state->length || length>state->length-state->offset)
    png_error(png,"Incomplete encoded PNG input");
  memcpy(buffer,state->input+state->offset,length);state->offset+=length;
}
static void decode_rows(struct DecodeState *state,uint32_t maximum) {
  png_uint_32 width,height;
  int depth,type,interlace,compression,filter;
  size_t y;
  png_read_info(state->png,state->info);
  png_get_IHDR(state->png,state->info,&width,&height,&depth,&type,&interlace,&compression,&filter);
  state->out->width=width;state->out->height=height;
  state->out->source_bit_depth=depth;state->out->source_color_type=type;state->out->source_interlace=interlace;
  if(!width || width!=height || width>maximum || (width&(width-1))) {
    state->out->status=1;
    snprintf(state->out->error,sizeof(state->out->error),"Original textures require a square power-of-two image, at most %u pixels",maximum);
    return;
  }
  /* Match the original wx PNG RGB sample path: palette/low-depth expansion,
   * gray->RGB, and high byte of 16-bit samples. Discard alpha, not hidden RGB.
   * No png_set_gamma, background composition or unpremultiplication is used. */
  png_set_expand(state->png);
  png_set_gray_to_rgb(state->png);
  png_set_strip_16(state->png);
  png_set_packing(state->png);
  png_set_strip_alpha(state->png);
  png_set_interlace_handling(state->png);
  png_read_update_info(state->png,state->info);
  if(png_get_bit_depth(state->png,state->info)!=8 || png_get_channels(state->png,state->info)!=3)
    png_error(state->png,"Unexpected packed RGB format");
  state->out->row_bytes=png_get_rowbytes(state->png,state->info);
  if(state->out->row_bytes!=(size_t)width*3 || height>SIZE_MAX/state->out->row_bytes)
    png_error(state->png,"Unexpected packed RGB layout");
  state->out->rgb_bytes=state->out->row_bytes*height;
  state->out->rgb=(unsigned char*)calloc(1,state->out->rgb_bytes);
  state->rows=(png_bytep*)malloc((size_t)height*sizeof(*state->rows));
  if(!state->out->rgb || !state->rows)png_error(state->png,"PNG pixel allocation failed");
  for(y=0;y<height;++y)state->rows[y]=state->out->rgb+y*state->out->row_bytes;
  png_read_image(state->png,state->rows);
  png_read_end(state->png,state->info);
  state->out->status=0;
}
void olex2_png_decode_memory(const unsigned char *bytes,size_t length,
                           uint32_t maximum,struct Olex2PngResult *out) {
  struct DecodeState *state;
  if(!out)return;
  memset(out,0,sizeof(*out));out->status=2;
  if(!bytes || length<8 || png_sig_cmp(bytes,0,8)) {
    snprintf(out->error,sizeof(out->error),"Expected an encoded PNG image");return;
  }
  state=(struct DecodeState*)calloc(1,sizeof(*state));
  if(!state) {snprintf(out->error,sizeof(out->error),"PNG state allocation failed");return;}
  state->input=bytes;state->length=length;state->out=out;
  state->png=png_create_read_struct_2(PNG_LIBPNG_VER_STRING,state,decoder_error,decoder_warning,state,decoder_alloc,decoder_free);
  if(!state->png) {snprintf(out->error,sizeof(out->error),"PNG owner creation failed");free(state);return;}
  if(setjmp(png_jmpbuf(state->png))==0) {
    state->info=png_create_info_struct(state->png);
    if(!state->info)png_error(state->png,"PNG metadata creation failed");
    png_set_read_fn(state->png,state,decoder_read);
    decode_rows(state,maximum);
  }
  png_destroy_read_struct(&state->png,&state->info,NULL);
  free(state->rows);
  if(out->status!=0) {free(out->rgb);out->rgb=NULL;out->rgb_bytes=0;}
  free(state);
}
void olex2_png_result_release(struct Olex2PngResult *result) {
  if(result) {free(result->rgb);result->rgb=NULL;}
}
const char *olex2_png_decoder_version(void) {return PNG_LIBPNG_VER_STRING;}
