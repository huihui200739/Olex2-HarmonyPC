#include "application_view.h"
#include "original_document_viewport.h"
#include "fpe_shader_adapter.h"
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {
constexpr size_t MaximumCoreJsonBytes=1024u*1024u;
std::string Quote(const std::string& text) {
  static constexpr char hex[]="0123456789abcdef";
  std::string out="\"";
  for (unsigned char c : text) {
    if(c=='"'||c=='\\') {out+='\\';out+=c;}
    else if(c<32) {out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
    else out+=c;
  }
  return out+'"';
}
std::string EmptyCore(const std::string& error, bool loaded=false, bool changed=false) {
  return std::string("{\"passed\":false,\"loaded\":")+(loaded?"true":"false")+
    ",\"name\":\"\",\"format\":\"\",\"byteCount\":0,\"sha256\":\"\",\"revision\":0,\"frames\":0,"
    "\"atomCount\":0,\"bondCount\":0,\"exportByteCount\":0,\"error\":"+Quote(error)+
    ",\"modelMayHaveChanged\":"+(changed?"true":"false")+
    ",\"source\":\"original TGXApp/TGXFile + original TGlRenderer\"}";
}
void Envelope(ApplicationViewResult& r,const std::string* rejectedCoreJson=nullptr) {
  r.json=std::string("{\"passed\":")+(r.passed?"true":"false")+
    ",\"surfacePresented\":"+(r.surfacePresented?"true":"false")+
    ",\"parentContextRestored\":"+(r.parentContextRestored?"true":"false")+
    ",\"sessionRetained\":"+(r.sessionRetained?"true":"false")+
    ",\"error\":"+Quote(r.error)+",\"core\":"+r.coreJson;
  // Preserve rejected bounded core bytes as diagnostic text, never as trusted
  // status. Validation failure must still keep the envelope/core failed.
  if(rejectedCoreJson)r.json+=",\"rejectedCoreJson\":"+Quote(*rejectedCoreJson);
  r.json+="}";
}
void Require(bool condition,const std::string& message) {
  if(!condition)throw std::runtime_error(message);
}
std::string EglError(const char* where) {
  std::ostringstream out;out<<where<<" (EGL 0x"<<std::hex<<eglGetError()<<')';return out.str();
}
void AppendError(std::string& error,const std::string& detail) {
  if(!error.empty())error+="; ";error+=detail;
}
// Core's private status is a flat JSON object, not arbitrary executable text.
// Parse the actual top-level fields; substring searches could mistake a quoted
// error or a filename for a successful loaded/passed state.
struct Scalar {char kind=0;std::string value;};
class CoreStatusParser {
  const std::string& text_;size_t p_=0;
  void Space(){while(p_<text_.size()&&(text_[p_]==' '||text_[p_]=='\n'||text_[p_]=='\r'||text_[p_]=='\t'))++p_;}
  char Take(){Require(p_<text_.size(),"Truncated original status JSON");return text_[p_++];}
  void Expect(char c){Space();Require(Take()==c,"Invalid original status JSON delimiter");}
  unsigned Hex(){unsigned n=0;for(int i=0;i<4;++i){char c=Take();n<<=4;if(c>='0'&&c<='9')n+=c-'0';else if(c>='a'&&c<='f')n+=c-'a'+10;else if(c>='A'&&c<='F')n+=c-'A'+10;else throw std::runtime_error("Invalid original status JSON escape");}return n;}
  static void Utf8(std::string& out,unsigned n){
    if(n<0x80)out+=char(n);
    else if(n<0x800){out+=char(0xc0|(n>>6));out+=char(0x80|(n&63));}
    else if(n<0x10000){out+=char(0xe0|(n>>12));out+=char(0x80|((n>>6)&63));out+=char(0x80|(n&63));}
    else{out+=char(0xf0|(n>>18));out+=char(0x80|((n>>12)&63));out+=char(0x80|((n>>6)&63));out+=char(0x80|(n&63));}
  }
  std::string String(){
    Expect('"');std::string out;
    for(;;){unsigned char c=Take();if(c=='"')return out;Require(c>=32,"Control character in original status JSON");
      if(c!='\\'){out+=char(c);continue;}
      switch(Take()){
        case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;
        case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
        case 'u':{unsigned n=Hex();if(n>=0xd800&&n<=0xdbff){Require(Take()=='\\'&&Take()=='u',"Unpaired JSON surrogate");unsigned low=Hex();Require(low>=0xdc00&&low<=0xdfff,"Invalid JSON surrogate");n=0x10000+((n-0xd800)<<10)+(low-0xdc00);}else Require(n<0xdc00||n>0xdfff,"Unpaired JSON surrogate");Utf8(out,n);break;}
        default:throw std::runtime_error("Invalid original status JSON escape");
      }
    }
  }
public:
  explicit CoreStatusParser(const std::string& text):text_(text){}
  std::map<std::string,Scalar> Parse(){
    std::map<std::string,Scalar> fields;Expect('{');Space();
    if(p_<text_.size()&&text_[p_]=='}'){++p_;return fields;}
    for(;;){std::string key=String();Expect(':');Space();Require(p_<text_.size(),"Missing original status value");Scalar scalar;
      if(text_[p_]=='"'){scalar.kind='s';scalar.value=String();}
      else{size_t start=p_;while(p_<text_.size()&&text_[p_]!=','&&text_[p_]!='}'&&text_[p_]!=' '&&text_[p_]!='\n'&&text_[p_]!='\r'&&text_[p_]!='\t')++p_;scalar.value=text_.substr(start,p_-start);
        if(scalar.value=="true"||scalar.value=="false")scalar.kind='b';
        // The current core uses -1 only for not-yet-picked pixel coordinates.
        // Keep every existing counter unsigned; do not accept general negatives.
        else if(scalar.value=="-1"&&(key=="lastPickPixelX"||key=="lastPickPixelY"))scalar.kind='i';
        else {scalar.kind='n';Require(!scalar.value.empty(),"Empty original status value");
          if(key=="bondRadius"){
            // The original bond radius is a real number (e.g. 0.2). Keep the
            // strict scalar grammar but allow one decimal point for this field.
            size_t dots=0,digits=0;for(char c:scalar.value){if(c=='.'){++dots;}else{Require(c>='0'&&c<='9',"Invalid original bond radius");++digits;}}
            Require(dots<=1 && digits>0 && scalar.value.front()!='.' && scalar.value.back()!='.',"Invalid original bond radius");
            Require(scalar.value=="0"||scalar.value[0]!='0'||scalar.value.size()>1,"Invalid original bond radius");
          }else{
            Require(scalar.value=="0"||scalar.value[0]!='0',"Invalid original status number");for(char c:scalar.value)Require(c>='0'&&c<='9',"Non-integer original status field");
          }}}
      Require(fields.emplace(std::move(key),std::move(scalar)).second,"Duplicate original status field");
      Space();char c=Take();if(c=='}')break;Require(c==',',"Invalid original status separator");
    }
    Space();Require(p_==text_.size(),"Trailing original status JSON");return fields;
  }
};
void ReadCore(ApplicationViewResult& result) {
  auto fields=CoreStatusParser(result.coreJson).Parse();
  auto field=[&](const char* key,char kind)->const std::string&{
    auto found=fields.find(key);Require(found!=fields.end()&&found->second.kind==kind,std::string("Missing/invalid original status field: ")+key);return found->second.value;
  };
  result.corePassed=field("passed",'b')=="true";result.loaded=field("loaded",'b')=="true";
  result.modelMayHaveChanged=field("modelMayHaveChanged",'b')=="true";
  for(const char* key:{"name","format","sha256","error","source"})field(key,'s');
  for(const char* key:{"byteCount","revision","frames","atomCount","bondCount","exportByteCount"})field(key,'n');
  // Selection metadata is absent before the first valid core response and may
  // be absent in honest failure envelopes. Never default a malformed field.
  auto optional=[&](const char* key,char kind)->const Scalar*{
    auto found=fields.find(key);if(found==fields.end())return nullptr;
    Require(found->second.kind==kind,std::string("Invalid original selection status field: ")+key);
    return &found->second;
  };
  for(const char* key:{"selectedObjectCount","selectionSequence","originalPickingCalls"}){
    if(const auto* value=optional(key,'n')){
      // JSON counters are consumed as JS numbers: reject overflow/precision loss.
      const std::string maximum="9007199254740991";
      Require(value->value.size()<maximum.size()||
        (value->value.size()==maximum.size()&&value->value<=maximum),
        std::string("Original selection counter exceeds safe integer range: ")+key);
    }
  }
  for(const char* key:{"lastPickPixelX","lastPickPixelY"}){
    auto found=fields.find(key);if(found==fields.end())continue;
    const Scalar& value=found->second;
    const std::string maximum="2147483647";
    Require((value.kind=='i'&&value.value=="-1")||
      (value.kind=='n'&&(value.value.size()<maximum.size()||
        (value.value.size()==maximum.size()&&value.value<=maximum))),
      std::string("Invalid original pick pixel coordinate: ")+key);
  }
  optional("pickHit",'b');optional("selectionAction",'s');optional("selectionSource",'s');
  if(const auto* collection=optional("lastPickedCollection",'s'))
    Require(collection->value.size()<=512,"Original picked collection name exceeds 512 UTF-8 bytes");
  if(!result.corePassed)result.error=field("error",'s').empty()?"Original document operation failed":field("error",'s');
  result.coreStateKnown=true;
}
struct SavedBinding {
  EGLDisplay display=eglGetCurrentDisplay();
  EGLContext context=eglGetCurrentContext();
  EGLSurface draw=eglGetCurrentSurface(EGL_DRAW),read=eglGetCurrentSurface(EGL_READ);
  EGLenum api=eglQueryAPI();
  bool Restore(EGLDisplay fallback) const {
    const bool apiOk=eglBindAPI(api)==EGL_TRUE;
    const EGLDisplay target=display==EGL_NO_DISPLAY?fallback:display;
    const bool bound=eglMakeCurrent(target,draw,read,context)==EGL_TRUE;
    return apiOk&&bound&&eglGetCurrentDisplay()==display&&eglGetCurrentContext()==context&&
      eglGetCurrentSurface(EGL_DRAW)==draw&&eglGetCurrentSurface(EGL_READ)==read&&eglQueryAPI()==api;
  }
};
struct HostCallbacks {
  EGLDisplay display=EGL_NO_DISPLAY;EGLSurface surface=EGL_NO_SURFACE;EGLContext context=EGL_NO_CONTEXT;
  int width=0,height=0;void* driver=nullptr;std::string shaderError;
};
thread_local HostCallbacks* currentHost=nullptr;
void* Raw(const char* name) {
  if(!currentHost||!currentHost->driver||!name)return nullptr;
  void* symbol=dlsym(currentHost->driver,name);
  return symbol?symbol:reinterpret_cast<void*>(eglGetProcAddress(name));
}
// Do not let C++ exceptions escape the C GL callback. The invalid shader forces
// an explicit original-core failure as well as retaining the adapter error.
void ShaderSource(GLuint shader,GLsizei count,const GLchar* const* strings,const GLint* lengths) noexcept {
  auto source=reinterpret_cast<decltype(&glShaderSource)>(Raw("glShaderSource"));
  auto shaderType=reinterpret_cast<decltype(&glGetShaderiv)>(Raw("glGetShaderiv"));
  if(!source||!shaderType||!currentHost)return;
  try {
    Require(count>0&&count<=4096&&strings,"Invalid generated shader source array");
    std::string input;
    for(GLsizei i=0;i<count;++i){Require(strings[i]!=nullptr,"Null generated shader string");
      size_t n=lengths&&lengths[i]>=0?size_t(lengths[i]):strnlen(strings[i],MaximumCoreJsonBytes+1);
      Require(n<=MaximumCoreJsonBytes&&input.size()<=MaximumCoreJsonBytes-n,"Generated shader source too large");input.append(strings[i],n);}
    GLint type=0;shaderType(shader,GL_SHADER_TYPE,&type);
    if(input.find("// FPE_Shader generated")!=std::string::npos)input=AdaptGeneratedFPE(input,type==GL_VERTEX_SHADER);
    const GLchar* text=input.data();GLint length=static_cast<GLint>(input.size());source(shader,1,&text,&length);
  }catch(const std::exception& e){currentHost->shaderError=e.what();const GLchar* invalid="#version 300 es\n#error Olex2_FPE_adapter_failed\n";source(shader,1,&invalid,nullptr);}
  catch(...){currentHost->shaderError="Unknown FPE shader adapter failure";const GLchar* invalid="#version 300 es\n#error Olex2_FPE_adapter_failed\n";source(shader,1,&invalid,nullptr);}
}
void* Resolve(const char* name) {
  if(!name)return nullptr;
  if(std::strcmp(name,"glShaderSource")==0)return reinterpret_cast<void*>(ShaderSource);
  return Raw(name);
}
void Size(int* width,int* height){if(width)*width=currentHost?currentHost->width:0;if(height)*height=currentHost?currentHost->height:0;}
int MakeCurrent(){return currentHost&&eglMakeCurrent(currentHost->display,currentHost->surface,currentHost->surface,currentHost->context)==EGL_TRUE;}
struct CallbackScope {
  HostCallbacks* previous=currentHost;
  explicit CallbackScope(HostCallbacks& host){currentHost=&host;}
  ~CallbackScope(){currentHost=previous;}
};
}

bool ValidateOriginalDocumentInput(const std::string& name,const std::string& bytes,std::string& error) {
  if(name.empty()||name.size()>240||name.find('\0')!=std::string::npos||name.find_first_of("/\\:")!=std::string::npos){error="Expected a document basename (1..240 UTF-8 bytes), not a path or URI";return false;}
  size_t dot=name.rfind('.');std::string extension=dot==std::string::npos?"":name.substr(dot);
  std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
  if(dot==0||(extension!=".cif"&&extension!=".ins"&&extension!=".res"&&extension!=".xyz"&&extension!=".pdb"&&extension!=".mol")){error="Original document format must be .cif, .ins, .res, .xyz, .pdb or .mol";return false;}
  if(bytes.empty()||bytes.size()>MaximumOriginalDocumentBytes||bytes.find('\0')!=std::string::npos){error="Expected nonempty original text bytes without NUL (maximum 8 MiB)";return false;}
  return true;
}
ApplicationView::ApplicationView():coreJson_(EmptyCore("No original document operation has run")){}
ApplicationViewResult ApplicationView::Failure(const std::string& error) const {
  ApplicationViewResult result;result.error=error;result.coreJson=coreJson_;result.loaded=loaded_;result.contextGeneration=generation_;result.sessionRetained=HasContext();Envelope(result);return result;
}
bool ApplicationView::CompatibleConfig(EGLDisplay display,EGLConfig& config,std::string& error) const {
  if(!HasContext())return true;
  if(display!=display_){error="Retained original document belongs to another EGL display";return false;}
  EGLint count=0;const EGLint attributes[]={EGL_CONFIG_ID,configId_,EGL_NONE};
  if(eglChooseConfig(display,attributes,&config,1,&count)!=EGL_TRUE||count!=1){error=EglError("Retained original document window config unavailable");return false;}
  return true;
}
ApplicationViewResult ApplicationView::Run(EGLDisplay mainDisplay,EGLSurface mainSurface,
    EGLContext mainContext,int width,int height,void* runtime,const std::string& directory,
    Olex2DocumentRequest request,const olex2::harmony::FontDisplayMetrics& fontDisplay) {
  ApplicationViewResult result=Failure("Original document operation did not complete");
  const bool closing=request.operation==Olex2DocumentOperation::Close;
  const bool selecting=request.operation==Olex2DocumentOperation::Pick||
    request.operation==Olex2DocumentOperation::ClearSelection||
    request.operation==Olex2DocumentOperation::SelectAll||request.operation==Olex2DocumentOperation::InvertSelection;
  const bool viewportRequest=request.operation==Olex2DocumentOperation::CaptureViewport||request.operation==Olex2DocumentOperation::RestoreViewport;
  if(viewportRequest){
    if(!HasContext()||!loaded_)return Failure("No loaded original viewport session is available");
    if(request.operation==Olex2DocumentOperation::RestoreViewport){
      try{olex2::harmony::ParseOriginalDocumentViewport(request.viewport_snapshot);}
      catch(const std::exception& e){return Failure(e.what());}
    }
  }
  // Interaction may only borrow the already-loaded document context. A failed
  // Open/Close or missing session must not create a replacement selection view.
  if(selecting){
    if(!HasContext()||!loaded_)return Failure("No loaded original document session is available for selection");
    if(!std::isfinite(request.pointer_x)||!std::isfinite(request.pointer_y)||
        request.pointer_x<0||request.pointer_x>1||request.pointer_y<0||request.pointer_y>1||request.selection_mode>2)
      return Failure("Invalid original selection mode or normalized viewport coordinate");
  }
  const bool onscreen=mainDisplay!=EGL_NO_DISPLAY&&mainSurface!=EGL_NO_SURFACE&&mainContext!=EGL_NO_CONTEXT&&width>0&&height>0;
  if(!runtime||directory.empty())return Failure("Original application runtime/private directory unavailable");
  std::string validation;
  if(request.operation==Olex2DocumentOperation::Open&&!ValidateOriginalDocumentInput(request.name,request.bytes,validation))return Failure(validation);
  if(!onscreen&&(!closing||!HasContext()))return Failure("Original document needs the actual main surface; no unrelated demo fallback");
  const EGLDisplay targetDisplay=HasContext()?display_:mainDisplay;
  SavedBinding parent;EGLSurface offscreen=cleanupSurface_;EGLSurface targetSurface=onscreen?mainSurface:EGL_NO_SURFACE;
  bool bound=false,coreCompleted=false,detached=true,cleanupOk=true,coreStatusReceived=false;
  std::string rejectedCoreJson;
  result.error.clear();result.coreStateKnown=false;
  try {
    Require(eglBindAPI(EGL_OPENGL_ES_API)==EGL_TRUE,EglError("Bind original GLES API"));
    if(HasContext()){
      Require(!onscreen||mainDisplay==display_,"Main display changed while original document context is retained");
      if(onscreen){EGLint configId=0;Require(eglQueryContext(mainDisplay,mainContext,EGL_CONFIG_ID,&configId)==EGL_TRUE,EglError("Main window config identity"));Require(configId==configId_,"Main surface config is incompatible with retained original document context");}
    }else{
      EGLint count=0;Require(eglQueryContext(mainDisplay,mainContext,EGL_CONFIG_ID,&configId_)==EGL_TRUE,EglError("Main context config identity"));
      const EGLint attributes[]={EGL_CONFIG_ID,configId_,EGL_NONE};
      Require(eglChooseConfig(mainDisplay,attributes,&config_,1,&count)==EGL_TRUE&&count==1,EglError("Original document compatible config"));
      Require(generation_!=std::numeric_limits<uint64_t>::max(),"Original context generation exhausted");
      const EGLint contextAttributes[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
      context_=eglCreateContext(mainDisplay,config_,EGL_NO_CONTEXT,contextAttributes);
      Require(context_!=EGL_NO_CONTEXT,EglError("Create retained original GLES3 context"));
      display_=mainDisplay;++generation_;
    }
    result.contextGeneration=generation_;
    if(!onscreen){
      EGLint types=0;Require(eglGetConfigAttrib(display_,config_,EGL_SURFACE_TYPE,&types)==EGL_TRUE&&(types&EGL_PBUFFER_BIT),"Retained original context config has no offscreen cleanup surface; runtime cleanup deferred");
      const EGLint dimensions[]={EGL_WIDTH,1,EGL_HEIGHT,1,EGL_NONE};
      if(offscreen==EGL_NO_SURFACE){offscreen=eglCreatePbufferSurface(display_,config_,dimensions);cleanupSurface_=offscreen;}
      Require(offscreen!=EGL_NO_SURFACE,EglError("Create original cleanup pbuffer"));targetSurface=offscreen;width=height=1;
    }
    Require(eglMakeCurrent(display_,targetSurface,targetSurface,context_)==EGL_TRUE,EglError("Bind retained original document context"));bound=true;
    EGLint actualWidth=0,actualHeight=0;
    Require(eglQuerySurface(display_,targetSurface,EGL_WIDTH,&actualWidth)==EGL_TRUE&&eglQuerySurface(display_,targetSurface,EGL_HEIGHT,&actualHeight)==EGL_TRUE&&actualWidth==width&&actualHeight==height,"Actual original document surface dimensions differ from requested dimensions");
    Require(eglGetCurrentContext()==context_&&eglGetCurrentSurface(EGL_DRAW)==targetSurface&&eglGetCurrentSurface(EGL_READ)==targetSurface,"Original document is not bound to the requested surface");
    if(!driver_)driver_=dlopen("libGLESv3.so",RTLD_NOW|RTLD_LOCAL);
    Require(driver_!=nullptr,"Cannot resolve real GLES3 driver for original document");
    HostCallbacks callbacks;callbacks.display=display_;callbacks.surface=targetSurface;callbacks.context=context_;
    callbacks.width=width;callbacks.height=height;callbacks.driver=driver_;
    CallbackScope callbackScope(callbacks);
    Require(Raw("glShaderSource")&&Raw("glGetShaderiv"),"Original GLES3 shader entry points unavailable");
    Require(sequence_!=std::numeric_limits<unsigned>::max(),"Original document request sequence exhausted");
    request.context_generation=generation_;
    // The borrowed export pointer never escapes this synchronous request.
    std::string exported,viewport;request.exportedBytes=request.operation==Olex2DocumentOperation::Export?&exported:nullptr;
    request.viewport_result=viewportRequest?&viewport:nullptr;
    int completed=0;Olex2LegacyHost host{};
    host.resolve=Resolve;host.framebuffer_size=Size;host.make_current=MakeCurrent;
    host.files_dir=directory.c_str();host.existing_runtime=runtime;host.run=++sequence_;
    host.completed=&completed;host.font_display_metrics=fontDisplay;host.document_request=&request;
    result.coreInvoked=true;
    const char* raw=olex2_legacy_run(&host);
    Require(raw!=nullptr,"Original document core returned no status");
    size_t length=strnlen(raw,MaximumCoreJsonBytes+1);Require(length<=MaximumCoreJsonBytes,"Original document status exceeds bound");
    result.coreJson.assign(raw,length);coreStatusReceived=true;ReadCore(result);coreJson_=result.coreJson;loaded_=result.loaded;
    coreCompleted=completed==1&&result.corePassed;
    Require(callbacks.shaderError.empty(),"Original FPE adapter: "+callbacks.shaderError);
    GLenum error=glGetError();Require(error==GL_NO_ERROR,"Original document GLES error "+std::to_string(error));
    Require(coreCompleted,result.error.empty()?"Original document core did not confirm completion":result.error);
    if(request.operation==Olex2DocumentOperation::Export){
      Require(!exported.empty()&&exported.size()<=MaximumOriginalDocumentBytes&&exported.find('\0')==std::string::npos,"Original saver returned invalid or oversized text bytes");result.exportedBytes=std::move(exported);
    }
    if(viewportRequest){
      // A separate bounded output; never parse/cache a viewport as core status.
      olex2::harmony::ParseOriginalDocumentViewport(viewport);result.viewportJson=std::move(viewport);
    }
    if(onscreen){result.surfacePresented=eglSwapBuffers(display_,targetSurface)==EGL_TRUE;Require(result.surfacePresented,EglError("Present actual original document surface"));}
  }catch(const std::exception& e){if(result.error.empty())result.error=e.what();else if(result.error!=e.what())AppendError(result.error,e.what());}
  catch(...){AppendError(result.error,"Unknown original document window failure");}
  if(result.coreInvoked&&!result.coreStateKnown){
    if(coreStatusReceived)rejectedCoreJson=result.coreJson;
    result.modelMayHaveChanged=true;result.coreJson=EmptyCore("Original core returned no valid current status",loaded_,true);coreJson_=result.coreJson;
  }
  // Detach first. Never destroy a temporary surface/context after restoring the
  // main dispatch; retained37 observed that ordering corrupt the emulator's GL.
  if(bound||offscreen!=EGL_NO_SURFACE||closing){
    detached=eglMakeCurrent(targetDisplay,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT)==EGL_TRUE;
    if(!detached){AppendError(result.error,EglError("Detach original document context"));cleanupOk=false;}
  }
  if(offscreen!=EGL_NO_SURFACE){
    if(detached&&eglDestroySurface(targetDisplay,offscreen)==EGL_TRUE){offscreen=EGL_NO_SURFACE;cleanupSurface_=EGL_NO_SURFACE;}
    else{AppendError(result.error,EglError("Dispose original cleanup pbuffer; owned surface retained for retry"));cleanupOk=false;}
  }
  const bool mayRetire=closing&&coreCompleted&&result.coreStateKnown&&!result.loaded&&detached&&cleanupOk&&result.error.empty();
  if(mayRetire&&HasContext()){
    if(eglDestroyContext(display_,context_)==EGL_TRUE){context_=EGL_NO_CONTEXT;result.contextRetired=true;config_=nullptr;configId_=0;}
    else{AppendError(result.error,EglError("Retire closed original context; owner cleanup deferred"));cleanupOk=false;}
  }
  bool mainRestored=true;
  if(onscreen){
    mainRestored=eglMakeCurrent(mainDisplay,mainSurface,mainSurface,mainContext)==EGL_TRUE&&
      eglGetCurrentDisplay()==mainDisplay&&eglGetCurrentContext()==mainContext&&
      eglGetCurrentSurface(EGL_DRAW)==mainSurface&&eglGetCurrentSurface(EGL_READ)==mainSurface;
    if(mainRestored)mainRestored=glGetString(GL_VERSION)!=nullptr;
    if(!mainRestored)AppendError(result.error,"Main application EGL binding/dispatch restoration failed");
  }
  const bool externalRestored=parent.Restore(targetDisplay);
  result.parentContextRestored=mainRestored&&externalRestored;
  if(!externalRestored)AppendError(result.error,"Caller's EGL binding/API restoration failed");
  if(result.contextRetired){display_=EGL_NO_DISPLAY;if(driver_){dlclose(driver_);driver_=nullptr;}}
  result.passed=coreCompleted&&result.coreStateKnown&&result.parentContextRestored&&cleanupOk&&result.error.empty()&&
    (onscreen?result.surfacePresented:closing&&result.contextRetired);
  if(closing&&result.loaded){result.passed=false;AppendError(result.error,"Original Close left a loaded document; runtime retained");}
  if(!result.passed)result.exportedBytes.clear();
  // Restore may have completed in the real core before a swap/parent-binding
  // failure. Retain its readback internally to synchronize controller camera,
  // but return a failed presentation envelope. Failed capture returns no data.
  if(!result.passed&&request.operation!=Olex2DocumentOperation::RestoreViewport)result.viewportJson.clear();
  result.sessionRetained=HasContext();
  Envelope(result,coreStatusReceived&&!result.coreStateKnown?&rejectedCoreJson:nullptr);return result;
}
