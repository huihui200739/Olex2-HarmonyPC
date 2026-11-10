#ifndef OLEX2_HARMONY_GL_PIXEL_STORE_SCOPE_H
#define OLEX2_HARMONY_GL_PIXEL_STORE_SCOPE_H

// Packed 2D unsigned-byte transfer state only. No claim about binding, active
// unit, 3D image layout, context lifetime or all other GL transfer parameters.
namespace olex2_harmony {
template<class GLApi> class ScopedPixelStore {
  unsigned int parameter_;
  int previous_=0;
public:
  ScopedPixelStore(unsigned int parameter,int desired):parameter_(parameter) {
    GLApi::get(parameter_,&previous_);
    GLApi::pixelStore(parameter_,desired);
  }
  ~ScopedPixelStore() noexcept {GLApi::pixelStore(parameter_,previous_);}
  ScopedPixelStore(const ScopedPixelStore&)=delete;
  ScopedPixelStore& operator=(const ScopedPixelStore&)=delete;
};
// Canonical packed 2D layout. Clear row length/skips while transferring and
// restore every caller value in reverse order, including on C++ scope exit.
template<class GLApi> class ScopedPackedPixels {
  ScopedPixelStore<GLApi> alignment_, rowLength_, skipRows_, skipPixels_;
public:
  ScopedPackedPixels(unsigned int alignment,unsigned int rowLength,
                    unsigned int skipRows,unsigned int skipPixels)
    :alignment_(alignment,1),rowLength_(rowLength,0),
     skipRows_(skipRows,0),skipPixels_(skipPixels,0) {}
  ScopedPackedPixels(const ScopedPackedPixels&)=delete;
  ScopedPackedPixels& operator=(const ScopedPackedPixels&)=delete;
};
}
#endif
