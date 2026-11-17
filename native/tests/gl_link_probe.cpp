// Link-only probe for fixed-function GL symbols used by upstream Olex2.
// Taking addresses (stored/read as volatile) preserves linker references.
// No GL/EGL function is called; a graphics context is not created.
#include <GL/gl.h>
#include <EGL/egl.h>

int main() {
  decltype(&glMatrixMode) volatile matrix = &glMatrixMode;
  decltype(&glLoadIdentity) volatile identity = &glLoadIdentity;
  decltype(&glNewList) volatile list = &glNewList;
  decltype(&glBegin) volatile begin = &glBegin;
  decltype(&glVertex3d) volatile vertex = &glVertex3d;
  decltype(&glEnd) volatile end = &glEnd;
  decltype(&glEndList) volatile end_list = &glEndList;
  decltype(&eglGetError) volatile egl_error = &eglGetError;
  return (matrix == nullptr) | (identity == nullptr) | (list == nullptr) |
         (begin == nullptr) | (vertex == nullptr) | (end == nullptr) |
         (end_list == nullptr) | (egl_error == nullptr);
}
