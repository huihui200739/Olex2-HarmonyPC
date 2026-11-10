#include "polygon_probe.h"
#include "primitive_polygons.h"
#include "original_matrices.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <hilog/log.h>
namespace {
// Keep native query output storage independent of saved state and handles.
// The requested values still use their exact GL-defined element count.
GLint IntegerState(GLenum name) {
  std::array<GLint,16> output{};glGetIntegerv(name,output.data());return output[0];
}
template<size_t N> void IntegerState(GLenum name,GLint (&output)[N]) {
  static_assert(N<=16);std::array<GLint,16> storage{};glGetIntegerv(name,storage.data());std::copy_n(storage.begin(),N,output);
}
template<size_t N> void FloatState(GLenum name,GLfloat (&output)[N]) {
  static_assert(N<=16);std::array<GLfloat,16> storage{};glGetFloatv(name,storage.data());std::copy_n(storage.begin(),N,output);
}
// Restore every binding/uniform/capability that this isolated FBO probe changes.
// No global SDK/context settings, separate desktop EGL route or saved view
// preferences are changed by this probe.
struct StateGuard {
  bool restored=false,operationsOk=true;
  GLuint fbo=0,texture=0,vao=0,buffer=0;
  GLint draw=0,read=0,readBuffer=0,drawBuffer=0,oldTexture=0,oldVao=0,oldBuffer=0,oldProgram=0,viewport[4]{};
  GLfloat clear[4]{};GLint mask[4]{};
  const GLenum caps[7]={GL_BLEND,GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_DITHER,GL_RASTERIZER_DISCARD};
  GLint enabled[7]{};
  GLuint program;
  std::array<GLint,7> locations;
  std::array<std::array<float,16>,7> uniforms{};
  void CheckError(const char* phase,unsigned item=0) {
    const GLenum error=glGetError();operationsOk=operationsOk && error==GL_NO_ERROR;
    if(error!=GL_NO_ERROR)OH_LOG_Print(LOG_APP,LOG_ERROR,0x3801,"Olex2Native","OLEX2_POLYGON_API phase=%{public}s item=%{public}u error=%{public}u program=%{public}u",phase,item,unsigned(error),program);
  }
  StateGuard(GLuint p,const std::array<GLint,7>& l):program(p),locations(l) {
    draw=IntegerState(GL_DRAW_FRAMEBUFFER_BINDING);read=IntegerState(GL_READ_FRAMEBUFFER_BINDING);
    readBuffer=IntegerState(GL_READ_BUFFER);drawBuffer=IntegerState(GL_DRAW_BUFFER0);
    OH_LOG_Print(LOG_APP,LOG_INFO,0x3801,"Olex2Native","OLEX2_POLYGON_STATE draw=%{public}d read=%{public}d drawBuffer=%{public}d readBuffer=%{public}d",draw,read,drawBuffer,readBuffer);
    oldTexture=IntegerState(GL_TEXTURE_BINDING_2D);oldVao=IntegerState(GL_VERTEX_ARRAY_BINDING);
    oldBuffer=IntegerState(GL_ARRAY_BUFFER_BINDING);oldProgram=IntegerState(GL_CURRENT_PROGRAM);
    IntegerState(GL_VIEWPORT,viewport);FloatState(GL_COLOR_CLEAR_VALUE,clear);IntegerState(GL_COLOR_WRITEMASK,mask);
    CheckError("capture-bindings");
    for(unsigned i=0;i<7;++i) {enabled[i]=IntegerState(caps[i]);CheckError("capture-cap",caps[i]);glGetUniformfv(program,locations[i],uniforms[i].data());CheckError("capture-uniform",i);}
    // On this emulator glGetBooleanv(GL_COLOR_WRITEMASK) returned 0,0,0,0
    // while the integer query returned the actual 1,1,1,1 write state. Use
    // integer queries consistently for both capture and restoration checks.
    OH_LOG_Print(LOG_APP,LOG_INFO,0x3801,"Olex2Native","OLEX2_POLYGON_MASK integer=%{public}d,%{public}d,%{public}d,%{public}d",mask[0],mask[1],mask[2],mask[3]);
  }
  void Restore() {
    if(restored)return;
    glUseProgram(program);
    for(unsigned i=0;i<7;++i) {
      if(i<2)glUniformMatrix4fv(locations[i],1,GL_FALSE,uniforms[i].data());
      else if(i<6)glUniform3fv(locations[i],1,uniforms[i].data());
      else glUniform1f(locations[i],uniforms[i][0]);
      CheckError("restore-uniform",i);
      if(enabled[i])glEnable(caps[i]);else glDisable(caps[i]);
      CheckError("restore-cap",caps[i]);
    }
    glUseProgram(oldProgram);CheckError("restore-program");
    if(draw==read)glBindFramebuffer(GL_FRAMEBUFFER,draw);
    else {glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);glBindFramebuffer(GL_READ_FRAMEBUFFER,read);}
    // Read/draw buffer selection belongs to the respective framebuffer. This
    // probe does not modify either selection; verify both after rebinding.
    glBindTexture(GL_TEXTURE_2D,oldTexture);glBindVertexArray(oldVao);glBindBuffer(GL_ARRAY_BUFFER,oldBuffer);
    glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glClearColor(clear[0],clear[1],clear[2],clear[3]);glColorMask(mask[0]!=0,mask[1]!=0,mask[2]!=0,mask[3]!=0);
    if(buffer)glDeleteBuffers(1,&buffer);if(vao)glDeleteVertexArrays(1,&vao);
    if(fbo)glDeleteFramebuffers(1,&fbo);if(texture)glDeleteTextures(1,&texture);
    CheckError("restore-bindings-resources");restored=true;
  }
  bool RestoreAndVerify() {
    Restore();bool same=true;GLint value=0;
    const GLenum bindingNames[8]={GL_DRAW_FRAMEBUFFER_BINDING,GL_READ_FRAMEBUFFER_BINDING,GL_TEXTURE_BINDING_2D,GL_VERTEX_ARRAY_BINDING,GL_ARRAY_BUFFER_BINDING,GL_CURRENT_PROGRAM,GL_READ_BUFFER,GL_DRAW_BUFFER0};
    const GLint expected[8]={draw,read,oldTexture,oldVao,oldBuffer,oldProgram,readBuffer,drawBuffer};
    for(unsigned i=0;i<8;++i) {
      value=IntegerState(bindingNames[i]);same=same && value==expected[i];
      if(value!=expected[i])OH_LOG_Print(LOG_APP,LOG_ERROR,0x3801,"Olex2Native","OLEX2_POLYGON_RESTORE binding=%{public}u actual=%{public}d expected=%{public}d",unsigned(bindingNames[i]),value,expected[i]);
    }
    GLint currentViewport[4]{};GLfloat currentClear[4]{};GLint currentMask[4]{};
    IntegerState(GL_VIEWPORT,currentViewport);FloatState(GL_COLOR_CLEAR_VALUE,currentClear);IntegerState(GL_COLOR_WRITEMASK,currentMask);
    for(unsigned i=0;i<4;++i) {
      const bool equal=currentViewport[i]==viewport[i] && currentClear[i]==clear[i] && currentMask[i]==mask[i];same=same && equal;
      if(!equal)OH_LOG_Print(LOG_APP,LOG_ERROR,0x3801,"Olex2Native","OLEX2_POLYGON_RESTORE channel=%{public}u viewport=%{public}d/%{public}d clear=%{public}f/%{public}f mask=%{public}d/%{public}d",i,currentViewport[i],viewport[i],double(currentClear[i]),double(clear[i]),currentMask[i],mask[i]);
    }
    for(unsigned i=0;i<7;++i) {
      GLint cap=0;cap=IntegerState(caps[i]);CheckError("verify-cap",caps[i]);same=same && cap==enabled[i];
      if(cap!=enabled[i])OH_LOG_Print(LOG_APP,LOG_ERROR,0x3801,"Olex2Native","OLEX2_POLYGON_RESTORE cap=%{public}u actual=%{public}d expected=%{public}d",unsigned(caps[i]),cap,enabled[i]);
      std::array<float,16> current{};glGetUniformfv(program,locations[i],current.data());
      const unsigned count=i<2?16:i<6?3:1;
      for(unsigned k=0;k<count;++k) {
        same=same && current[k]==uniforms[i][k];
        if(current[k]!=uniforms[i][k])OH_LOG_Print(LOG_APP,LOG_ERROR,0x3801,"Olex2Native","OLEX2_POLYGON_RESTORE uniform=%{public}u component=%{public}u actual=%{public}f expected=%{public}f",i,k,double(current[k]),double(uniforms[i][k]));
      }
    }
    const GLenum error=glGetError();
    if(error!=GL_NO_ERROR)OH_LOG_Print(LOG_APP,LOG_ERROR,0x3801,"Olex2Native","OLEX2_POLYGON_RESTORE error=%{public}u",unsigned(error));
    return operationsOk && same && error==GL_NO_ERROR;
  }
  ~StateGuard() {Restore();}
};
}
PolygonGpuProbe RunOriginalPolygonGpuProbe(GLuint program) {
  PolygonGpuProbe result;
  TArrayList<vec3f> positions,normals;
  for(const auto& p:{vec3f(-.8f,-.8f,0),vec3f(.8f,-.8f,0),vec3f(.8f,.8f,0),vec3f(.2f,.8f,0),vec3f(.2f,-.2f,0),vec3f(-.2f,-.2f,0),vec3f(-.2f,.8f,0),vec3f(-.8f,.8f,0)})positions.Add(p);
  normals.Add(vec3f(0,0,1));const auto mesh=olex2::harmony::BuildUncoloredPolygonTriangles(positions,normals);
  result.inputVertices=mesh.sourceVertices;result.triangles=mesh.triangles;result.normalCommands=mesh.normalCommands;result.colorCommands=mesh.colorCommands;
  const char* names[7]={"mvp","normalTransform","ambientColor","diffuseColor","specularColor","emissionColor","shininess"};
  std::array<GLint,7> locations{};
  for(unsigned i=0;i<7;++i) {locations[i]=glGetUniformLocation(program,names[i]);if(locations[i]<0){result.error="Polygon probe uniform missing";return result;}}
  {
    StateGuard guard(program,locations);
    glGenFramebuffers(1,&guard.fbo);glBindFramebuffer(GL_FRAMEBUFFER,guard.fbo);
    glGenTextures(1,&guard.texture);glBindTexture(GL_TEXTURE_2D,guard.texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,guard.texture,0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) {result.error="Polygon probe framebuffer incomplete";return result;}
    glGenVertexArrays(1,&guard.vao);glBindVertexArray(guard.vao);glGenBuffers(1,&guard.buffer);glBindBuffer(GL_ARRAY_BUFFER,guard.buffer);
    using olex2::harmony::MeshVertex;
    glBufferData(GL_ARRAY_BUFFER,mesh.vertices.size()*sizeof(MeshVertex),mesh.vertices.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(MeshVertex),reinterpret_cast<void*>(offsetof(MeshVertex,position)));glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(MeshVertex),reinterpret_cast<void*>(offsetof(MeshVertex,normal)));glEnableVertexAttribArray(1);
    for(GLenum cap:guard.caps)glDisable(cap);
    glViewport(0,0,64,64);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program);const auto identity=olex2::harmony::IdentityMatrix();
    glUniformMatrix4fv(locations[0],1,GL_FALSE,identity.data());glUniformMatrix4fv(locations[1],1,GL_FALSE,identity.data());
    for(unsigned i=2;i<5;++i)glUniform3f(locations[i],0,0,0);glUniform3f(locations[5],.25f,.75f,.5f);glUniform1f(locations[6],0);
    glDrawArrays(GL_TRIANGLES,0,mesh.vertices.size());
    const unsigned points[6][3]={{12,44,1},{51,44,1},{32,12,1},{32,48,0},{32,62,0},{32,1,0}};
    bool passed=mesh.sourceVertices==8 && mesh.triangles==6 && mesh.normalCommands==1 && mesh.colorCommands==0;
    for(unsigned i=0;i<6;++i) {
      GLubyte rgba[4]{};glReadPixels(points[i][0],points[i][1],1,1,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
      for(unsigned k=0;k<3;++k)result.samples[i][k]=points[i][k];
      const int expected[4]={points[i][2]?64:0,points[i][2]?191:0,points[i][2]?128:0,255};
      for(unsigned k=0;k<4;++k) {result.samples[i][k+3]=rgba[k];passed=passed && std::abs(int(rgba[k])-expected[k])<=1;}
    }
    result.verified=passed && glGetError()==GL_NO_ERROR;
    result.stateRestored=guard.RestoreAndVerify();
    result.verified=result.verified && result.stateRestored;
  }
  result.verified=result.verified && glGetError()==GL_NO_ERROR;
  if(!result.verified)result.error="Original polygon/concave tessellation GPU pixel or state-restore check failed";
  return result;
}
