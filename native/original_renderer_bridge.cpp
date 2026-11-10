#include "original_renderer_host.h"
#include "application_runtime.h"
#include "original_interpreter.h"
#include "glrender.h"
#include "glprimitive.h"
#include "gdrawobject.h"
#include "gl4esinit.h"
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
std::string result;
struct Counts { unsigned current=0, start=0, end=0, orient=0, dimensions=0, destroyed=0;float projection[16]={},modelview[16]={}; };
std::string Quoted(const std::string& s) {
  std::ostringstream o;o << '"';
  for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<c;else if(c>=32)o<<c;else o<<"\\u00"<<"0123456789abcdef"[c>>4]<<"0123456789abcdef"[c&15];}
  o << '"';return o.str();
}
class WindowScene final:public AGlScene {
  const Olex2LegacyHost& host;Counts& counts;
  TGlFont& DoCreateFont(TGlFont&,bool) const override {throw std::runtime_error("Fonts are not implemented by this isolated window probe");}
public:
  WindowScene(const Olex2LegacyHost& h,Counts& c):host(h),counts(c){SetEnabled(true);}
  ~WindowScene() override {++counts.destroyed;}
  void ScaleFonts(double) override {throw std::runtime_error("Font scaling unsupported in probe");}
  void RestoreFontScale() override {throw std::runtime_error("Font scaling unsupported in probe");}
  olxstr ShowFontDialog(TGlFont*,const olxstr&) override {throw std::runtime_error("Font dialog unsupported in probe");}
  bool MakeCurrent() override {++counts.current;return IsEnabled() && host.make_current()!=0;}
  bool StartDraw() override {++counts.start;return AGlScene::StartDraw();}
  void EndDraw() override {++counts.end;AGlScene::EndDraw();}
};
class ShapeObject final:public AGDrawObject {
  Counts& counts;
public:
  ShapeObject(TGlRenderer& r,Counts& c):AGDrawObject(r,"WindowProbe"),counts(c){}
  bool Orient(TGlPrimitive&) override {++counts.orient;glGetFloatv(GL_PROJECTION_MATRIX,counts.projection);glGetFloatv(GL_MODELVIEW_MATRIX,counts.modelview);return false;}
  bool GetDimensions(vec3d& max,vec3d& min) override {++counts.dimensions;max=vec3d(.24,.24,.24);min=vec3d(-.24,-.24,-.24);return true;}
};
void CheckGL(const char* stage){unsigned e=glGetError();if(e)throw std::runtime_error(std::string(stage)+" GL error "+std::to_string(e));}
void Array(std::ostringstream& o,const float* p,size_t n){o<<'[';for(size_t i=0;i<n;++i){if(i)o<<',';o<<p[i];}o<<']';}
struct CompatibilityScope {
  CompatibilityScope(const Olex2LegacyHost& h){set_getprocaddress(h.resolve);set_getmainfbsize(h.framebuffer_size);initialize_gl4es();}
  ~CompatibilityScope(){close_gl4es();set_getprocaddress(nullptr);set_getmainfbsize(nullptr);}
};
}
extern "C" const char* olex2_legacy_run(const Olex2LegacyHost* ptr) {
  std::ostringstream out;bool completed=false;
  if(ptr && ptr->completed)*ptr->completed=0;
  try {
    if(!ptr || !ptr->resolve || !ptr->make_current || !ptr->framebuffer_size || !ptr->files_dir || !ptr->existing_runtime)throw std::invalid_argument("Incomplete window host ABI");
    const auto& h=*ptr;int w=0,hh=0;h.framebuffer_size(&w,&hh);
    if(w<64 || hh<64 || w>4096 || hh>4096 || !h.make_current())throw std::runtime_error("Invalid actual window surface");
    auto& runtime=*static_cast<olex2::harmony::ApplicationRuntime*>(h.existing_runtime);
    const auto runtimeStatus=runtime.Status();
    if(!TBasicApp::HasInstance() || runtimeStatus.baseDir!=TBasicApp::GetBaseDir().ToMBStr().c_str())throw std::runtime_error("Original singleton and borrowed runtime directories differ");
    olex2::harmony::OriginalInterpreterGuard originalScope;
    if(!runtimeStatus.initialized || !runtimeStatus.originalCoreInitialized || !runtimeStatus.originalCoreEmptyFile)throw std::runtime_error("Original empty application core did not initialize");
    CompatibilityScope compatibility(h);
    CheckGL("compatibility initialize");
    out << "{\"run\":" << h.run << ",\"width\":" << w << ",\"height\":" << hh
        << ",\"shared_existing_runtime\":true,\"original_empty_core\":true,\"runtime_dir\":" << Quoted(runtimeStatus.baseDir)
        << ",\"legacy_version\":" << Quoted(reinterpret_cast<const char*>(glGetString(GL_VERSION)))
        << ",\"legacy_renderer\":" << Quoted(reinterpret_cast<const char*>(glGetString(GL_RENDERER))) << ",\"cases\":[";
    const short types[]={sgloTriangles,sgloSphere,sgloDisk};
    const char* names[]={"triangle","glu-sphere","glu-disk"};
    for(unsigned mode=0;mode<3;++mode){
      Counts counts;float projection[16]={},modelview[16]={};bool compiled=false;
      {
        // Ownership of scene transfers to the genuine renderer. Objects remain
        // alive until its collections have been cleared by its destructor.
        auto scene=std::make_unique<WindowScene>(h,counts);
        std::unique_ptr<ShapeObject> object;
        auto renderer=std::make_unique<TGlRenderer>(scene.get(),w,hh);scene.release();
        renderer->SetSceneComplete(false);
        auto& collection=renderer->NewCollection("WindowProbe");
        auto& primitive=collection.NewPrimitive(names[mode],types[mode]);
        TGlMaterial material;material.SetFlags(sglmEmissionFB);
        material.EmissionF=material.EmissionB=mode==0?TGlOption(1.f,0.f,0.f,1.f):mode==1?TGlOption(0.f,1.f,0.f,1.f):TGlOption(0.f,0.f,1.f,1.f);
        primitive.SetProperties(material);
        if(mode==0){primitive.Vertices.Add(vec3f(-.22f,-.20f,0));primitive.Vertices.Add(vec3f(.22f,-.20f,0));primitive.Vertices.Add(vec3f(0,.22f,0));primitive.Normals.Add(vec3f(0,0,1));}
        if(mode==1){primitive.Params[0]=.22;primitive.Params[1]=32;primitive.Params[2]=16;}
        if(mode==2){primitive.Params[0]=.09;primitive.Params[1]=.22;primitive.Params[2]=32;primitive.Params[3]=4;}
        object=std::make_unique<ShapeObject>(*renderer,counts);collection.AddObject(*object);
        // Patch 0016 initializes this original short-valued default. Check it
        // before explicitly configuring the isolated synthetic drawing scene.
        if(renderer->LightModel.GetLight(0).GetSpotExponent()!=0)throw std::runtime_error("Original light default exponent differs from zero");
        renderer->LightModel.GetLight(0).SetSpotExponent(0);
        renderer->LightModel.GetLight(0).SetSpotDirection(TGlOption(0.f,0.f,-1.f,0.f));
        {
          std::ofstream diagnostic(std::string(h.files_dir)+"/light-configured-run-"+std::to_string(h.run)+"-"+names[mode]+".txt");
          diagnostic<<"phase=after_explicit_scene_configuration\nspot_exponent="<<renderer->LightModel.GetLight(0).GetSpotExponent()<<"\nspot_direction="<<renderer->LightModel.GetLight(0).GetSpotDirection()[0]<<','<<renderer->LightModel.GetLight(0).GetSpotDirection()[1]<<','<<renderer->LightModel.GetLight(0).GetSpotDirection()[2]<<'\n';
        }
        renderer->InitLights();CheckGL("original InitLights with explicit scene parameters");
        renderer->Initialise();CheckGL("original renderer initialise");compiled=primitive.IsCompiled();
        glClearColor(10.f/255,15.f/255,20.f/255,1);
        renderer->Draw();glFinish();CheckGL("original renderer draw");
        glGetFloatv(GL_PROJECTION_MATRIX,projection);glGetFloatv(GL_MODELVIEW_MATRIX,modelview);CheckGL("original matrix readback");
        if(h.gpu_snapshot)h.gpu_snapshot(h.run,names[mode]);
        std::vector<unsigned char> pixels(static_cast<size_t>(w)*hh*4);
        glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,hh,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());CheckGL("actual pixel readback");
        const std::string path=std::string(h.files_dir)+"/legacy-run-"+std::to_string(h.run)+"-"+names[mode]+".rgba";
        std::ofstream file(path,std::ios::binary|std::ios::trunc);file.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());file.close();if(!file)throw std::runtime_error("Cannot retain window pixels");
        renderer.reset(); // its Clear/GLU/list destruction must run while current
        object.reset();CheckGL("original renderer destruction");
      }
      if(mode)out<<',';
      out<<"{\"name\":"<<Quoted(names[mode])<<",\"original_primitive_type\":"<<types[mode]<<",\"display_list_compiled\":"<<(compiled?"true":"false")
         <<",\"make_current\":"<<counts.current<<",\"start_draw\":"<<counts.start<<",\"end_draw\":"<<counts.end<<",\"object_orient\":"<<counts.orient<<",\"get_dimensions\":"<<counts.dimensions<<",\"scene_destroyed\":"<<counts.destroyed<<",\"projection\":";
      Array(out,counts.projection,16);out<<",\"modelview\":";Array(out,counts.modelview,16);out<<",\"post_draw_projection\":";Array(out,projection,16);out<<",\"post_draw_modelview\":";Array(out,modelview,16);out<<'}';
    }
    out<<"],\"execution_completed\":true,\"pixel_validation\":\"pending independent host analysis\",\"error\":\"\"}";
    result=out.str();completed=true;
  }catch(const TExceptionBase& e){result="{\"execution_completed\":false,\"error\":"+Quoted(e.GetException()->GetFullMessage().ToMBStr().c_str())+"}";
  }catch(const std::exception& e){result="{\"execution_completed\":false,\"error\":"+Quoted(e.what())+"}";
  }catch(...){result="{\"execution_completed\":false,\"error\":\"Unknown original renderer exception\"}";}
  if(ptr && ptr->completed)*ptr->completed=completed?1:0;
  return result.c_str();
}
