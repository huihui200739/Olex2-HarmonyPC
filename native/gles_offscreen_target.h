#pragma once
// The pinned compatibility headers default to X11 on Unix. Harmony uses
// opaque EGL handles and has no X11 native display or window dependency.
#if defined(__OHOS__) && !defined(EGL_NO_X11)
#define EGL_NO_X11 1
#endif
#include <EGL/egl.h>
#include <cstdint>
#include <thread>
#include <vector>

namespace olex2::harmony {
// Borrows the application's ES3 context; owns only a color texture, framebuffer
// and depth renderbuffer. Its calls must stay on that context's owning thread.
class GlesOffscreenTarget {
  EGLDisplay display_=EGL_NO_DISPLAY;
  EGLContext context_=EGL_NO_CONTEXT;
  uint32_t framebuffer_=0,color_=0,depth_=0;
  int32_t previousDraw_=0,previousRead_=0,previousTexture_=0,previousRenderbuffer_=0,previousActiveTexture_=0;
  int32_t previousViewport_[4]={0,0,0,0};
  std::thread::id thread_;
  int width_=0,height_=0;
  void RequireOwner() const;
public:
  GlesOffscreenTarget()=default;
  ~GlesOffscreenTarget() noexcept;
  GlesOffscreenTarget(const GlesOffscreenTarget&)=delete;
  GlesOffscreenTarget& operator=(const GlesOffscreenTarget&)=delete;
  void CreateFromCurrent(int width,int height);
  bool MakeCurrent();
  void Close();
  bool IsOpen() const {return framebuffer_!=0;}
  int Width() const {return width_;}
  int Height() const {return height_;}
  // RGBA8, bottom row first. Leaves the selected draw/read framebuffers and
  // pixel-pack buffer/alignment/row-length/skip settings unchanged.
  std::vector<uint8_t> ReadRGBA();
};
}
