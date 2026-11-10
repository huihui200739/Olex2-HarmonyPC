#include "original_renderer_host.h"
#include "shared_original_probe.h"
#include "fpe_shader_adapter.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <native_window/external_window.h>
#include <napi/native_api.h>
#include <hilog/log.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
std::recursive_mutex mutex;
std::string root;
unsigned run=0;int width=0,height=0;void* driver=nullptr;
EGLDisplay display=EGL_NO_DISPLAY;EGLContext legacy=EGL_NO_CONTEXT;EGLSurface surface=EGL_NO_SURFACE;
void* Raw(const char* n){void* p=dlsym(driver,n);return p?p:reinterpret_cast<void*>(eglGetProcAddress(n));}
std::ofstream trace;
void TraceCompile(GLuint shader){reinterpret_cast<decltype(&glCompileShader)>(Raw("glCompileShader"))(shader);GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);char log[8192]={};glGetShaderInfoLog(shader,sizeof(log),nullptr,log);trace<<"compile "<<shader<<" status="<<ok<<" log="<<log<<'\n';trace.flush();}
void TraceLink(GLuint program){reinterpret_cast<decltype(&glLinkProgram)>(Raw("glLinkProgram"))(program);GLint ok=0,uniforms=0,attributes=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);glGetProgramiv(program,GL_ACTIVE_UNIFORMS,&uniforms);glGetProgramiv(program,GL_ACTIVE_ATTRIBUTES,&attributes);char log[8192]={};glGetProgramInfoLog(program,sizeof(log),nullptr,log);trace<<"link "<<program<<" status="<<ok<<" uniforms="<<uniforms<<" attributes="<<attributes<<" log="<<log<<'\n';trace.flush();}
void TraceDrawArrays(GLenum mode,GLint first,GLsizei count){trace<<"drawArrays mode="<<mode<<" first="<<first<<" count="<<count<<'\n';reinterpret_cast<decltype(&glDrawArrays)>(Raw("glDrawArrays"))(mode,first,count);trace.flush();}
void TraceDrawElements(GLenum mode,GLsizei count,GLenum type,const void* indices){trace<<"drawElements mode="<<mode<<" count="<<count<<" type="<<type<<'\n';reinterpret_cast<decltype(&glDrawElements)>(Raw("glDrawElements"))(mode,count,type,indices);trace.flush();}
void TraceMask(GLboolean r,GLboolean g,GLboolean b,GLboolean a){trace<<"colorMask "<<unsigned(r)<<','<<unsigned(g)<<','<<unsigned(b)<<','<<unsigned(a)<<'\n';reinterpret_cast<decltype(&glColorMask)>(Raw("glColorMask"))(r,g,b,a);trace.flush();}
void TraceShaderSource(GLuint shader,GLsizei count,const GLchar*const* strings,const GLint* lengths){
 std::string input;for(GLsizei i=0;i<count;++i){if(lengths&&lengths[i]>=0)input.append(strings[i],lengths[i]);else input.append(strings[i]);}
 std::ofstream original(root+"/submitted-shader-run-"+std::to_string(run)+"-"+std::to_string(shader)+".txt");original<<input;original.close();
 std::string transformed=input;GLint type=0;glGetShaderiv(shader,GL_SHADER_TYPE,&type);
 // Bounded prototype: only GL4ES's generated fixed-pipeline shaders are
 // upgraded. User shaders and feature-detection probes are left untouched.
 if(input.find("// FPE_Shader generated")!=std::string::npos){
  transformed=AdaptGeneratedFPE(input,type==GL_VERTEX_SHADER);
  std::ofstream adapted(root+"/adapted-shader-run-"+std::to_string(run)+"-"+std::to_string(shader)+".txt");adapted<<transformed;trace<<"FPE ESSL300 adaptation shader="<<shader<<" type="<<type<<'\n';
 }
 const GLchar* text=transformed.c_str();const GLint length=transformed.size();reinterpret_cast<decltype(&glShaderSource)>(Raw("glShaderSource"))(shader,1,&text,&length);
}
void* Resolve(const char* n){if(!std::strcmp(n,"glShaderSource"))return reinterpret_cast<void*>(TraceShaderSource);if(!std::strcmp(n,"glCompileShader"))return reinterpret_cast<void*>(TraceCompile);if(!std::strcmp(n,"glLinkProgram"))return reinterpret_cast<void*>(TraceLink);if(!std::strcmp(n,"glDrawArrays"))return reinterpret_cast<void*>(TraceDrawArrays);if(!std::strcmp(n,"glDrawElements"))return reinterpret_cast<void*>(TraceDrawElements);if(!std::strcmp(n,"glColorMask"))return reinterpret_cast<void*>(TraceMask);return Raw(n);}
void Size(int* w,int* h){*w=width;*h=height;}
int MakeCurrent(){return eglMakeCurrent(display,surface,surface,legacy)==EGL_TRUE;}
void Require(bool ok,const char* s){if(!ok)throw std::runtime_error(std::string(s)+" EGL="+std::to_string(eglGetError())+" GL="+std::to_string(glGetError()));}
void GLCheck(const char* s){GLenum e=glGetError();if(e)throw std::runtime_error(std::string(s)+" GL="+std::to_string(e));}
std::string Quote(const std::string& s){std::ostringstream o;o<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<c;else if(c>=32)o<<c;else o<<' ';}o<<'"';return o.str();}
struct SavedBinding {
 EGLDisplay d=eglGetCurrentDisplay();EGLSurface draw=eglGetCurrentSurface(EGL_DRAW),read=eglGetCurrentSurface(EGL_READ);EGLContext c=eglGetCurrentContext();
 GLint viewport[4]={};
 SavedBinding(){if(c!=EGL_NO_CONTEXT)glGetIntegerv(GL_VIEWPORT,viewport);}
 bool Restore(bool restoreViewport=true)const {if(eglMakeCurrent(d==EGL_NO_DISPLAY?display:d,draw,read,c)!=EGL_TRUE)return false;if(restoreViewport&&c!=EGL_NO_CONTEXT)glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);return true;}
 void RestoreViewport()const{if(c!=EGL_NO_CONTEXT)glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);}
 bool Matches()const{return eglGetCurrentDisplay()==d&&eglGetCurrentSurface(EGL_DRAW)==draw&&eglGetCurrentSurface(EGL_READ)==read&&eglGetCurrentContext()==c;}
};
void ActualGPUSnapshot(unsigned sequence,const char* label){
 const GLenum beforeError=glGetError();std::ostringstream o;GLint program=0,fbo=0,vp[4]={},scissor[4]={};glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_FRAMEBUFFER_BINDING,&fbo);glGetIntegerv(GL_VIEWPORT,vp);glGetIntegerv(GL_SCISSOR_BOX,scissor);GLboolean masks[4]={};glGetBooleanv(GL_COLOR_WRITEMASK,masks);
 o<<"{\"program\":"<<program<<",\"framebuffer\":"<<fbo<<",\"viewport\":["<<vp[0]<<','<<vp[1]<<','<<vp[2]<<','<<vp[3]<<"],\"scissor\":["<<scissor[0]<<','<<scissor[1]<<','<<scissor[2]<<','<<scissor[3]<<"],\"scissor_enabled\":"<<unsigned(glIsEnabled(GL_SCISSOR_TEST))<<",\"depth_test\":"<<unsigned(glIsEnabled(GL_DEPTH_TEST))<<",\"cull_face\":"<<unsigned(glIsEnabled(GL_CULL_FACE))<<",\"color_write\":["<<unsigned(masks[0])<<','<<unsigned(masks[1])<<','<<unsigned(masks[2])<<','<<unsigned(masks[3])<<"],\"uniforms\":[";
 if(program){GLint count=0,linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);char info[8192]={};glGetProgramInfoLog(program,sizeof(info),nullptr,info);o<<"],\"link_status\":"<<linked<<",\"program_info\":"<<Quote(info)<<",\"active_uniform_values\":[";glGetProgramiv(program,GL_ACTIVE_UNIFORMS,&count);for(GLint i=0;i<count;++i){char name[256]={};GLsizei length=0;GLint size=0;GLenum type=0;glGetActiveUniform(program,i,sizeof(name),&length,&size,&type,name);GLint location=glGetUniformLocation(program,name);float values[16]={};glGetUniformfv(program,location,values);if(i)o<<',';o<<"{\"name\":"<<Quote(name)<<",\"type\":"<<type<<",\"size\":"<<size<<",\"values\":[";int components=type==GL_FLOAT_MAT4?16:type==GL_FLOAT_MAT3?9:type==GL_FLOAT_VEC4?4:type==GL_FLOAT_VEC3?3:type==GL_FLOAT_VEC2?2:1;for(int j=0;j<components;++j){if(j)o<<',';o<<values[j];}o<<"]}";}
 if(std::strncmp(label,"parent",6)!=0){GLuint shaders[4]={};GLsizei actual=0;glGetAttachedShaders(program,4,&actual,shaders);for(int i=0;i<actual;++i){GLint length=0;glGetShaderiv(shaders[i],GL_SHADER_SOURCE_LENGTH,&length);if(length>0&&length<262144){std::vector<char> text(length+1);glGetShaderSource(shaders[i],text.size(),nullptr,text.data());std::ofstream shader(root+"/actual-shader-run-"+std::to_string(sequence)+"-"+label+"-"+std::to_string(i)+".txt");shader<<text.data();}}}}
 o<<"],\"driver_error_before_snapshot\":"<<beforeError<<",\"driver_error\":"<<glGetError()<<'}';std::ofstream file(root+"/actual-gpu-run-"+std::to_string(sequence)+"-"+label+".json");file<<o.str()<<'\n';
}
std::string Read(const std::string& path){std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot read own GPU state capture");return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};}
}
SharedOriginalWindowResult RunSharedOriginalWindow(EGLDisplay mainDisplay,EGLSurface mainSurface,EGLContext mainContext,int w,int h,void* runtime,const std::string& directory){
 std::lock_guard<std::recursive_mutex> lock(mutex);SharedOriginalWindowResult output;++run;output.run=run;
 const std::string artifactDirectory="original-renderer-pid-"+std::to_string(getpid())+"-run-"+std::to_string(run);
 root=directory+"/"+artifactDirectory;display=mainDisplay;surface=mainSurface;width=w;height=h;
 SavedBinding parent;std::string legacyReport="null",error,beforeState,afterState;bool restored=false,swapped=false,artifactCreated=false,isolated=false,surfaceVerified=false;int completed=0;
 try{
  // Synthetic debug artifacts use the existing debug-file readability so
  // the explicit HDC target can retrieve them; no user data is placed here.
  artifactCreated=mkdir(root.c_str(),0755)==0;
  Require(artifactCreated,"Create exclusive current-process artifact directory");
  Require(legacy==EGL_NO_CONTEXT,"Previous temporary original context was not disposed");
  Require(runtime && parent.d==display && parent.c==mainContext && parent.draw==surface && parent.read==surface,"Actual main application context must be current");
  ActualGPUSnapshot(run,"parent-before");beforeState=Read(root+"/actual-gpu-run-"+std::to_string(run)+"-parent-before.json");
  EGLint configId=0,count=0;EGLConfig config=nullptr;Require(eglQueryContext(display,mainContext,EGL_CONFIG_ID,&configId),"main context config identity");const EGLint attrs[]={EGL_CONFIG_ID,configId,EGL_NONE};Require(eglChooseConfig(display,attrs,&config,1,&count)&&count==1,"same main window config");
  const EGLint contextAttrs[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};legacy=eglCreateContext(display,config,EGL_NO_CONTEXT,contextAttrs);Require(legacy!=EGL_NO_CONTEXT&&legacy!=mainContext&&MakeCurrent(),"isolated original-renderer context on actual main surface");isolated=true;
  EGLint actualW=0,actualH=0;Require(eglQuerySurface(display,surface,EGL_WIDTH,&actualW)&&eglQuerySurface(display,surface,EGL_HEIGHT,&actualH)&&actualW==width&&actualH==height,"actual main surface dimensions");
  Require(eglGetCurrentSurface(EGL_DRAW)==mainSurface&&eglGetCurrentSurface(EGL_READ)==mainSurface,"Actual original renderer surface identity");surfaceVerified=true;
  driver=dlopen("libGLESv3.so",RTLD_NOW|RTLD_LOCAL);Require(driver!=nullptr,"real GLES driver");trace.open(root+"/actual-driver-trace-run-"+std::to_string(run)+".log");
  Olex2LegacyHost host{Resolve,Size,MakeCurrent,root.c_str(),nullptr,0,runtime,run,ActualGPUSnapshot,&completed};legacyReport=olex2_legacy_run(&host);trace.close();GLCheck("real driver after original renderer");
  swapped=eglSwapBuffers(display,surface)==EGL_TRUE;Require(swapped,"present original rendering on main window");
 }catch(const std::exception& e){error=e.what();}
 if(trace.is_open())trace.close();
 // Dispose of the temporary context while no context is bound, then bind
 // the surviving main context. Do not destroy another context after restoring
 // the main dispatch: the current emulator left that dispatch unusable despite
 // reporting the parent EGL handles unchanged (retained failed candidates).
 bool detached=true,destroyed=true;
 if(legacy!=EGL_NO_CONTEXT){
  detached=eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT)==EGL_TRUE;
  if(detached){destroyed=eglDestroyContext(display,legacy)==EGL_TRUE;if(destroyed)legacy=EGL_NO_CONTEXT;}
  else destroyed=false;
 }
 if((!detached||!destroyed)&&error.empty())error="Temporary original context disposal failed";
 if(parent.Restore()&&parent.Matches()){
  try{if(artifactCreated){ActualGPUSnapshot(run,"parent-after");afterState=Read(root+"/actual-gpu-run-"+std::to_string(run)+"-parent-after.json");restored=!beforeState.empty()&&beforeState==afterState;}}catch(const std::exception& e){if(error.empty())error=e.what();}
 }else if(error.empty())error="Main application EGL binding restoration failed";
 if(driver){dlclose(driver);driver=nullptr;}
 if(!restored&&error.empty())error="Selected actual main GPU state was not restored";
 output.completed=completed==1&&restored&&swapped&&error.empty();
 std::ostringstream out;out<<"{\"run\":"<<run<<",\"pid\":"<<getpid()<<",\"artifact_subdirectory\":"<<Quote(artifactDirectory)<<",\"shared_existing_runtime\":"<<(completed==1?"true":"false")<<",\"same_main_window_surface\":"<<(surfaceVerified?"true":"false")<<",\"original_context_isolated\":"<<(isolated?"true":"false")<<",\"swapped\":"<<(swapped?"true":"false")<<",\"temporary_context_detached\":"<<(detached?"true":"false")<<",\"temporary_context_destroyed\":"<<(destroyed?"true":"false")<<",\"parent_state_restored\":"<<(restored?"true":"false")<<",\"legacy\":"<<legacyReport<<",\"error\":"<<Quote(error)<<'}';output.json=out.str();
 if(artifactCreated){std::ofstream file(root+"/shared-original-window-run-"+std::to_string(run)+".json");file<<output.json<<'\n';}return output;
}
