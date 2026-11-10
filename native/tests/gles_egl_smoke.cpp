// Standalone GLES3/EGL diagnostic. Offscreen output is not a GUI/app test.
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <cstring>
#include <iostream>

namespace {
GLuint shader(GLenum kind, const char* source) {
  GLuint id = glCreateShader(kind);
  glShaderSource(id, 1, &source, nullptr);
  glCompileShader(id);
  GLint compiled = GL_FALSE;
  glGetShaderiv(id, GL_COMPILE_STATUS, &compiled);
  if (compiled != GL_TRUE) {
    char log[2048] = {};
    glGetShaderInfoLog(id, sizeof(log), nullptr, log);
    std::cerr << "FAIL shader compile: " << log << '\n';
    glDeleteShader(id);
    return 0;
  }
  return id;
}

bool pixel_is(GLubyte red, GLubyte green, GLubyte blue) {
  GLubyte pixel[4] = {};
  glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
  GLenum error = glGetError();
  std::cout << "PIXEL " << int(pixel[0]) << ',' << int(pixel[1]) << ','
            << int(pixel[2]) << ',' << int(pixel[3]) << '\n';
  return error == GL_NO_ERROR && pixel[0] == red && pixel[1] == green &&
         pixel[2] == blue && pixel[3] == 255;
}
}

int main(int argc, char** argv) {
  decltype(&glCreateShader) volatile create = &glCreateShader;
  decltype(&glGenBuffers) volatile buffers = &glGenBuffers;
  decltype(&eglInitialize) volatile initialize = &eglInitialize;
  if (!create || !buffers || !initialize) {
    return 1;
  }
  std::cout << "PASS GLES3/EGL runtime symbol loading\n" << std::flush;
  if (argc == 2 && std::strcmp(argv[1], "--symbols-only") == 0) {
    std::cout << "NOT_TESTED graphics context and rendering\n";
    return 0;
  }
  if (argc != 1) {
    std::cerr << "Usage: olex2_gles_egl_smoke [--symbols-only]\n";
    return 2;
  }
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint major = 0, minor = 0;
  if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor)) {
    std::cerr << "FAIL default-display EGL initialization, error=0x"
              << std::hex << eglGetError() << '\n';
    return 3;
  }
  EGLSurface surface = EGL_NO_SURFACE;
  EGLContext context = EGL_NO_CONTEXT;
  GLuint vertex = 0, fragment = 0, program = 0;
  auto cleanup = [&]() {
    if (program) glDeleteProgram(program);
    if (vertex) glDeleteShader(vertex);
    if (fragment) glDeleteShader(fragment);
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
    if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
    eglTerminate(display);
  };
  const EGLint attributes[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
  EGLConfig config = nullptr;
  EGLint count = 0;
  if (!eglChooseConfig(display, attributes, &config, 1, &count) || count == 0 ||
      !eglBindAPI(EGL_OPENGL_ES_API)) {
    std::cerr << "FAIL GLES3 pbuffer config, EGL error=0x" << std::hex << eglGetError() << '\n';
    cleanup();
    return 4;
  }
  const EGLint surface_attributes[] = {EGL_WIDTH, 8, EGL_HEIGHT, 8, EGL_NONE};
  const EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  surface = eglCreatePbufferSurface(display, config, surface_attributes);
  context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
  if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT ||
      !eglMakeCurrent(display, surface, surface, context)) {
    std::cerr << "FAIL GLES3 pbuffer/context creation, EGL error=0x" << std::hex << eglGetError() << '\n';
    cleanup();
    return 5;
  }
  std::cout << "PASS EGL " << major << '.' << minor << " offscreen context\n";
  const GLubyte* version = glGetString(GL_VERSION);
  std::cout << "GL_VERSION " << (version ? reinterpret_cast<const char*>(version) : "unknown") << '\n';
  glViewport(0, 0, 8, 8);
  glClearColor(1.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);
  if (!pixel_is(255, 0, 0)) {
    std::cerr << "FAIL red clear/readback\n";
    cleanup();
    return 6;
  }
  std::cout << "PASS red clear/readback\n";
  const char* vertex_source =
    "#version 300 es\n"
    "void main() {\n"
    " const vec2 p[3] = vec2[3](vec2(-1.,-1.),vec2(3.,-1.),vec2(-1.,3.));\n"
    " gl_Position=vec4(p[gl_VertexID],0.,1.);\n} \n";
  const char* fragment_source =
    "#version 300 es\nprecision mediump float;\n"
    "out vec4 color;\nvoid main(){color=vec4(0.,1.,0.,1.);}\n";
  vertex = shader(GL_VERTEX_SHADER, vertex_source);
  fragment = shader(GL_FRAGMENT_SHADER, fragment_source);
  if (!vertex || !fragment) {
    cleanup();
    return 7;
  }
  program = glCreateProgram();
  glAttachShader(program, vertex);
  glAttachShader(program, fragment);
  glLinkProgram(program);
  GLint linked = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (!linked) {
    std::cerr << "FAIL shader program link\n";
    cleanup();
    return 8;
  }
  glUseProgram(program);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  if (!pixel_is(0, 255, 0)) {
    std::cerr << "FAIL green shader draw/readback\n";
    cleanup();
    return 9;
  }
  std::cout << "PASS green shader draw/readback\n";
  cleanup();
  std::cout << "PASS standalone offscreen diagnostic; Olex2 renderer/GUI NOT_TESTED\n";
  return 0;
}
