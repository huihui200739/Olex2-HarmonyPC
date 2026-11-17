// Positive modern desktop GL/EGL link-only control, not a rendering test.
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <EGL/egl.h>

int main() {
  decltype(&glGetString) volatile query = &glGetString;
  decltype(&glClear) volatile clear = &glClear;
  decltype(&glCreateShader) volatile shader = &glCreateShader;
  decltype(&glGenBuffers) volatile buffer = &glGenBuffers;
  decltype(&eglGetError) volatile egl_error = &eglGetError;
  return (query == nullptr) | (clear == nullptr) | (shader == nullptr) |
         (buffer == nullptr) | (egl_error == nullptr);
}
