#include "line_path_probe.h"
#include "primitive_line_paths.h"
#include "sha.h"
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>
namespace olex2::harmony {
namespace {
GLint Integer(GLenum name){std::array<GLint,16> a{};glGetIntegerv(name,a.data());return a[0];}
template<size_t N>void Integer(GLenum name,std::array<GLint,N>& value){std::array<GLint,16>a{};glGetIntegerv(name,a.data());std::copy_n(a.begin(),N,value.begin());}
template<size_t N>void Float(GLenum name,std::array<float,N>& value){std::array<float,16>a{};glGetFloatv(name,a.data());std::copy_n(a.begin(),N,value.begin());}
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::string Digest(const void* bytes,size_t count){std::string out;const std::string grouped=SHA256::Digest(static_cast<const char*>(bytes),count).c_str();for(unsigned char c:grouped)if(c!=' ')out+=static_cast<char>(std::tolower(c));return out;}
struct Guard {
 GLuint fbo=0,texture=0,vao=0,vbo=0,ebo=0,program=0,vertexShader=0,fragmentShader=0;
 GLint draw=0,read=0,readBuffer=0,drawBuffer=0,oldTexture=0,oldActiveTexture=0,oldVao=0,oldVbo=0,oldEbo=0,oldProgram=0,packAlignment=0,packRowLength=0,packSkipPixels=0,packSkipRows=0,oldPackBuffer=0,oldUnpackBuffer=0;
 std::array<GLint,4> viewport{},mask{};std::array<float,4> clear{};std::array<float,1> width{};
 const std::array<GLenum,9> caps{{GL_BLEND,GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_DITHER,GL_RASTERIZER_DISCARD,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE}};
 std::array<GLint,9> enabled{};bool restored=false,ok=true;
 void Check(){ok=glGetError()==GL_NO_ERROR && ok;}
 Guard(){
  draw=Integer(GL_DRAW_FRAMEBUFFER_BINDING);read=Integer(GL_READ_FRAMEBUFFER_BINDING);readBuffer=Integer(GL_READ_BUFFER);drawBuffer=Integer(GL_DRAW_BUFFER0);
  oldTexture=Integer(GL_TEXTURE_BINDING_2D);oldActiveTexture=Integer(GL_ACTIVE_TEXTURE);oldVao=Integer(GL_VERTEX_ARRAY_BINDING);oldVbo=Integer(GL_ARRAY_BUFFER_BINDING);oldEbo=Integer(GL_ELEMENT_ARRAY_BUFFER_BINDING);oldProgram=Integer(GL_CURRENT_PROGRAM);packAlignment=Integer(GL_PACK_ALIGNMENT);packRowLength=Integer(GL_PACK_ROW_LENGTH);packSkipPixels=Integer(GL_PACK_SKIP_PIXELS);packSkipRows=Integer(GL_PACK_SKIP_ROWS);oldPackBuffer=Integer(GL_PIXEL_PACK_BUFFER_BINDING);oldUnpackBuffer=Integer(GL_PIXEL_UNPACK_BUFFER_BINDING);
  Integer(GL_VIEWPORT,viewport);Integer(GL_COLOR_WRITEMASK,mask);Float(GL_COLOR_CLEAR_VALUE,clear);Float(GL_LINE_WIDTH,width);
  for(size_t i=0;i<caps.size();++i)enabled[i]=Integer(caps[i]);Check();
 }
 void Restore(){
  if(restored)return;
  glUseProgram(oldProgram);
  if(draw==read)glBindFramebuffer(GL_FRAMEBUFFER,draw);else{glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);glBindFramebuffer(GL_READ_FRAMEBUFFER,read);}
  glBindTexture(GL_TEXTURE_2D,oldTexture);glBindVertexArray(oldVao);glBindBuffer(GL_ARRAY_BUFFER,oldVbo);glBindBuffer(GL_PIXEL_PACK_BUFFER,oldPackBuffer);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,oldUnpackBuffer);glPixelStorei(GL_PACK_ALIGNMENT,packAlignment);glPixelStorei(GL_PACK_ROW_LENGTH,packRowLength);glPixelStorei(GL_PACK_SKIP_PIXELS,packSkipPixels);glPixelStorei(GL_PACK_SKIP_ROWS,packSkipRows);
  glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glClearColor(clear[0],clear[1],clear[2],clear[3]);glColorMask(mask[0]!=0,mask[1]!=0,mask[2]!=0,mask[3]!=0);glLineWidth(width[0]);
  for(size_t i=0;i<caps.size();++i)if(enabled[i])glEnable(caps[i]);else glDisable(caps[i]);
  if(ebo)glDeleteBuffers(1,&ebo);if(vbo)glDeleteBuffers(1,&vbo);if(vao)glDeleteVertexArrays(1,&vao);
  if(fbo)glDeleteFramebuffers(1,&fbo);if(texture)glDeleteTextures(1,&texture);
  if(program)glDeleteProgram(program);if(vertexShader)glDeleteShader(vertexShader);if(fragmentShader)glDeleteShader(fragmentShader);
  Check();restored=true;
 }
 bool RestoreAndVerify(){
  Restore();bool same=ok;
  const std::array<GLenum,16> names{{GL_DRAW_FRAMEBUFFER_BINDING,GL_READ_FRAMEBUFFER_BINDING,GL_READ_BUFFER,GL_DRAW_BUFFER0,GL_TEXTURE_BINDING_2D,GL_ACTIVE_TEXTURE,GL_VERTEX_ARRAY_BINDING,GL_ARRAY_BUFFER_BINDING,GL_ELEMENT_ARRAY_BUFFER_BINDING,GL_CURRENT_PROGRAM,GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_PIXELS,GL_PACK_SKIP_ROWS,GL_PIXEL_PACK_BUFFER_BINDING,GL_PIXEL_UNPACK_BUFFER_BINDING}};
  const std::array<GLint,16> expected{{draw,read,readBuffer,drawBuffer,oldTexture,oldActiveTexture,oldVao,oldVbo,oldEbo,oldProgram,packAlignment,packRowLength,packSkipPixels,packSkipRows,oldPackBuffer,oldUnpackBuffer}};
  for(size_t i=0;i<names.size();++i)same=Integer(names[i])==expected[i] && same;
  std::array<GLint,4> v{},m{};std::array<float,4> c{};std::array<float,1>w{};Integer(GL_VIEWPORT,v);Integer(GL_COLOR_WRITEMASK,m);Float(GL_COLOR_CLEAR_VALUE,c);Float(GL_LINE_WIDTH,w);
  same=v==viewport && m==mask && c==clear && w==width && same;
  for(size_t i=0;i<caps.size();++i)same=Integer(caps[i])==enabled[i] && same;
  Check();return same && ok;
 }
 ~Guard(){Restore();}
};
void Compile(GLuint shader,const char* text){glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);Require(ok==GL_TRUE,"Original path isolated shader compile failed");}
struct MapRead {
 void* data=nullptr;bool closed=false;
 explicit MapRead(GLsizeiptr size){data=glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER,0,size,GL_MAP_READ_BIT);Require(data!=nullptr,"Cannot map actual path index buffer");}
 bool Close(){const bool success=glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER)==GL_TRUE;closed=true;return success;}
 ~MapRead(){if(!closed && data)glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);}
};
}
LinePathGpuProbe RunOriginalLinePathGpuProbe(){
 LinePathGpuProbe result;
 const EGLContext oldContext=eglGetCurrentContext();const EGLDisplay oldDisplay=eglGetCurrentDisplay();const EGLSurface oldDraw=eglGetCurrentSurface(EGL_DRAW),oldRead=eglGetCurrentSurface(EGL_READ);
 if(oldContext==EGL_NO_CONTEXT){result.error="Original path GPU probe requires an actual current application context";return result;}
 try{
  Guard guard;Require(guard.ok,"GL error while capturing original path probe state");
  const char* vs="#version 300 es\nlayout(location=0) in vec3 p; void main(){gl_Position=vec4(p,1.0);}";
  const char* fs="#version 300 es\nprecision highp float; out vec4 color; void main(){color=vec4(68.0/255.0,136.0/255.0,204.0/255.0,1.0);}";
  guard.vertexShader=glCreateShader(GL_VERTEX_SHADER);guard.fragmentShader=glCreateShader(GL_FRAGMENT_SHADER);Require(guard.vertexShader && guard.fragmentShader,"Cannot allocate original path shaders");Compile(guard.vertexShader,vs);Compile(guard.fragmentShader,fs);
  guard.program=glCreateProgram();Require(guard.program!=0,"Cannot allocate original path program");glAttachShader(guard.program,guard.vertexShader);glAttachShader(guard.program,guard.fragmentShader);glLinkProgram(guard.program);GLint linked=0;glGetProgramiv(guard.program,GL_LINK_STATUS,&linked);Require(linked==GL_TRUE,"Original path isolated shader link failed");
  Require(Integer(GL_TRANSFORM_FEEDBACK_ACTIVE)==GL_FALSE,"Original path probe requires no active transform feedback");
  glBindBuffer(GL_PIXEL_PACK_BUFFER,0);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_PACK_ALIGNMENT,1);glPixelStorei(GL_PACK_ROW_LENGTH,0);glPixelStorei(GL_PACK_SKIP_PIXELS,0);glPixelStorei(GL_PACK_SKIP_ROWS,0);
  glGenTextures(1,&guard.texture);Require(guard.texture!=0,"Cannot allocate path texture");glBindTexture(GL_TEXTURE_2D,guard.texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
  glGenFramebuffers(1,&guard.fbo);Require(guard.fbo!=0,"Cannot allocate path FBO");glBindFramebuffer(GL_FRAMEBUFFER,guard.fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,guard.texture,0);Require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Original path FBO is incomplete");Require(Integer(GL_SAMPLE_BUFFERS)==0,"Original path probe requires a single-sampled FBO");
  glGenVertexArrays(1,&guard.vao);Require(guard.vao!=0,"Cannot allocate path VAO");glBindVertexArray(guard.vao);glGenBuffers(1,&guard.vbo);Require(guard.vbo!=0,"Cannot allocate path VBO");glBindBuffer(GL_ARRAY_BUFFER,guard.vbo);glGenBuffers(1,&guard.ebo);Require(guard.ebo!=0,"Cannot allocate path EBO");glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,guard.ebo);
  glUseProgram(guard.program);for(GLenum cap:guard.caps)glDisable(cap);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glViewport(0,0,64,64);glClearColor(0,0,0,1);glLineWidth(1);
  // Exact binary fractions place each synthetic edge through pixel centers.
  // LinePathGpuSample 3x3 interior regions (not endpoints): ES3 permits a one-pixel
  // deviation from ideal diamond-exit rasterization, not arbitrary geometry.
  const std::vector<std::array<float,3>> positions{{{-.734375f,-.734375f,0}},{{.765625f,-.734375f,0}},{{.765625f,.765625f,0}},{{-.734375f,.765625f,0}}};
  glBufferData(GL_ARRAY_BUFFER,positions.size()*sizeof(positions[0]),positions.data(),GL_STATIC_DRAW);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(positions[0]),nullptr);glEnableVertexAttribArray(0);
  for(auto kind:{OriginalLinePathKind::Strip,OriginalLinePathKind::Loop}){
   const auto capture=CaptureOriginalLinePath(positions,{},kind);LinePathGpuCase output;output.kind=kind==OriginalLinePathKind::Strip?"strip":"loop";output.inputVertices=positions.size();output.indexCount=capture.indices.size();
   glBufferData(GL_ELEMENT_ARRAY_BUFFER,capture.indices.size()*sizeof(uint32_t),capture.indices.data(),GL_STATIC_DRAW);guard.Check();Require(guard.ok,"Original path buffer upload failed");
   // Query target-specific buffer metadata and actual mapped GPU bytes.
   GLint size=0;glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER,GL_BUFFER_SIZE,&size);Require(size==GLint(capture.indices.size()*sizeof(uint32_t)),"Actual path EBO byte count differs");
   std::array<uint32_t,8> actual{};Require(capture.indices.size()<=actual.size(),"Path readback exceeds the fixed synthetic contour budget");{MapRead map(size);std::memcpy(actual.data(),map.data,size);Require(map.Close(),"Cannot unmap actual path index buffer");}
   output.actualIndices.assign(actual.begin(),actual.begin()+capture.indices.size());output.actualBufferVerified=output.actualIndices==capture.indices;output.indexSha256=Digest(actual.data(),size);Require(output.actualBufferVerified,"Actual path EBO differs from original output topology");
   glClear(GL_COLOR_BUFFER_BIT);glDrawElements(GL_LINES,output.indexCount,GL_UNSIGNED_INT,nullptr);glFinish();std::array<float,1> width{};Float(GL_LINE_WIDTH,width);output.actualLineWidth=width[0];Require(output.actualLineWidth==1,"Original path probe width must remain one");
   const char* names[]={"bottom","right","top","closing_left","interior","outside","outside_closing_band","inside_closing_band"};const std::array<std::array<int,2>,8> centers{{{{32,8}},{{56,32}},{{32,56}},{{8,32}},{{32,32}},{{2,32}},{{5,32}},{{11,32}}}};
   bool allPixels=true;
   for(size_t i=0;i<centers.size();++i){LinePathGpuSample sample;sample.name=names[i];sample.x=centers[i][0];sample.y=centers[i][1];sample.expectedLit=i<3 || (i==3 && kind==OriginalLinePathKind::Loop);glReadPixels(sample.x-1,sample.y-1,3,3,GL_RGBA,GL_UNSIGNED_BYTE,sample.rgba.data());bool anyLit=false,allClear=true,allKnown=true;
    for(size_t p=0;p<9;++p){const auto* c=sample.rgba.data()+4*p;bool lit=std::abs(int(c[0])-68)<=1 && std::abs(int(c[1])-136)<=1 && std::abs(int(c[2])-204)<=1 && c[3]==255;const bool clear=c[0]==0 && c[1]==0 && c[2]==0 && c[3]==255;anyLit=anyLit || lit;allClear=allClear && clear;allKnown=allKnown && (lit || clear);}
    sample.passed=allKnown && (sample.expectedLit?anyLit:allClear);allPixels=allPixels && sample.passed;output.samples.push_back(sample);
   }
   guard.Check();output.pixelsVerified=allPixels && guard.ok;Require(output.pixelsVerified,"Original path independent edge/interior/closure pixels differ");result.cases.push_back(output);
  }
  result.stateRestored=guard.RestoreAndVerify();result.contextRestored=eglGetCurrentContext()==oldContext && eglGetCurrentDisplay()==oldDisplay && eglGetCurrentSurface(EGL_DRAW)==oldDraw && eglGetCurrentSurface(EGL_READ)==oldRead;result.verified=result.cases.size()==2 && result.stateRestored && result.contextRestored;Require(result.verified,"Original path probe did not restore caller state/context");
 }catch(const std::exception& e){result.verified=false;result.error=e.what();}
 return result;
}
}
