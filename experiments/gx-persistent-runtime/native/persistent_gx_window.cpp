#include "original_renderer_host.h"
#include "window_texture_1d_probe.h"
#include "application_runtime.h"
#include "persistent_gx_window.h"
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
struct Counts { unsigned current=0, start=0, end=0, orient=0, dimensions=0, destroyed=0;float projection[16]={},modelview[16]={}; };
std::string Quoted(const std::string& s) {
  std::ostringstream o;o << '"';
  for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<c;else if(c>=32)o<<c;else o<<"\\u00"<<"0123456789abcdef"[c>>4]<<"0123456789abcdef"[c&15];}
  o << '"';return o.str();
}
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
namespace olex2::harmony {
namespace {
// Declared after the probe object, so exception unwinding retires original
// renderer collections/lists before releasing the object they refer to.
class GXCaseCleanup {
  TGlRenderer& renderer_;PlatformSceneLifetime& lifetime_;bool closed_=false;
public:
  GXCaseCleanup(TGlRenderer& renderer,PlatformSceneLifetime& lifetime):renderer_(renderer),lifetime_(lifetime){}
  void Close(){if(!closed_){closed_=true;try{renderer_.Clear();}catch(...){lifetime_.cleanupFailed=true;throw;}}}
  ~GXCaseCleanup(){try{Close();}catch(...){lifetime_.cleanupFailed=true;}}
};
// This lease always ends before CompatibilityScope closes GL4ES. Clear and
// texture preservation must happen with the leased context still current.
class GXSurfaceLease {
  TGlRenderer& renderer_;PlatformWindowScene& scene_;PlatformSceneLifetime& lifetime_;bool closed_=false;
public:
  GXSurfaceLease(TGlRenderer& renderer,PlatformWindowScene& scene,PlatformSceneLifetime& lifetime,const Olex2LegacyHost& host)
    :renderer_(renderer),scene_(scene),lifetime_(lifetime){scene_.Bind(host);}
  void Close(){
    if(closed_)return;
    closed_=true;
    try{renderer_.BeforeContextChange();}
    catch(...){lifetime_.cleanupFailed=true;scene_.Unbind();throw;}
    scene_.Unbind();
  }
  ~GXSurfaceLease(){try{Close();}catch(...){lifetime_.cleanupFailed=true;}}
};
}
std::string RunPersistentGXWindowLocked(const Olex2LegacyHost& h,const ApplicationRuntimeStatus& runtimeStatus,
    TGXApp& app,PlatformWindowScene& scene,PlatformSceneLifetime& lifetime){
    if(!h.resolve || !h.make_current || !h.framebuffer_size || !h.files_dir)
      throw std::invalid_argument("Incomplete persistent window host ABI");
    int w=0,hh=0;h.framebuffer_size(&w,&hh);
    if(w<64 || hh<64 || w>4096 || hh>4096 || !h.make_current())throw std::runtime_error("Invalid actual window surface");
    auto& genuine=app.GetRenderer();
    const bool uniqueOwner=TBasicApp::HasInstance() && &TBasicApp::GetInstance()==static_cast<TBasicApp*>(&app)
      && &TXApp::GetInstance()==static_cast<TXApp*>(&app) && &TGXApp::GetInstance()==&app;
    if(!uniqueOwner || &genuine.GetScene()!=&scene || scene.Parent()!=&genuine || scene.IsBound() || scene.IsEnabled() || lifetime.cleanupFailed)
      throw std::runtime_error("Persistent original GX owner or detached scene identity differs");
    const auto bindingBefore=lifetime.bindings,detachmentBefore=lifetime.detachments;
    std::ostringstream out;
    CompatibilityScope compatibility(h);
    GXSurfaceLease lease(genuine,scene,lifetime,h);
    if(!scene.MakeCurrent())throw std::runtime_error("Cannot bind persistent platform scene");
    genuine.AfterContextChange();genuine.Resize(w,hh);CheckGL("persistent renderer attach");
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
        // The genuine TGXApp already owns renderer and scene. Each temporary
        // diagnostic object outlives its renderer collections on all exits.
        const auto callbacksBefore=lifetime;
        std::unique_ptr<ShapeObject> object;
        auto* renderer=&genuine;
        GXCaseCleanup caseCleanup(genuine,lifetime);
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
        caseCleanup.Close(); // Retire collections/lists, but keep the renderer and scene.
        object.reset();CheckGL("persistent renderer case cleanup");
        counts.current=static_cast<unsigned>(lifetime.makeCurrent-callbacksBefore.makeCurrent);
        counts.start=static_cast<unsigned>(lifetime.startDraw-callbacksBefore.startDraw);
        counts.end=static_cast<unsigned>(lifetime.endDraw-callbacksBefore.endDraw);
        counts.destroyed=static_cast<unsigned>(lifetime.destroyed-callbacksBefore.destroyed);
      }
      if(mode)out<<',';
      out<<"{\"name\":"<<Quoted(names[mode])<<",\"original_primitive_type\":"<<types[mode]<<",\"display_list_compiled\":"<<(compiled?"true":"false")
         <<",\"make_current\":"<<counts.current<<",\"start_draw\":"<<counts.start<<",\"end_draw\":"<<counts.end<<",\"object_orient\":"<<counts.orient<<",\"get_dimensions\":"<<counts.dimensions<<",\"scene_destroyed\":"<<counts.destroyed<<",\"projection\":";
      Array(out,counts.projection,16);out<<",\"modelview\":";Array(out,counts.modelview,16);out<<",\"post_draw_projection\":";Array(out,projection,16);out<<",\"post_draw_modelview\":";Array(out,modelview,16);out<<'}';
    }
    out<<"],\"texture_1d\":"<<RunOriginalTexture1DWindowProbe(h,w,hh);
    lease.Close();CheckGL("persistent renderer detach before compatibility shutdown");
    const bool ownerAfter=&TGXApp::GetInstance()==&app && &app.GetRenderer()==&genuine && &genuine.GetScene()==&scene;
    if(!ownerAfter || scene.IsBound() || scene.IsEnabled() || lifetime.cleanupFailed)
      throw std::runtime_error("Persistent renderer did not release its surface lease");
    out<<",\"persistent_gx\":{\"unique_owner\":"<<(uniqueOwner?"true":"false")
       <<",\"owner_unchanged\":"<<(ownerAfter?"true":"false")
       <<",\"owner_address\":"<<reinterpret_cast<uintptr_t>(&app)
       <<",\"renderer_address\":"<<reinterpret_cast<uintptr_t>(&genuine)
       <<",\"scene_address\":"<<reinterpret_cast<uintptr_t>(&scene)
       <<",\"binding_before\":"<<bindingBefore<<",\"binding_after\":"<<lifetime.bindings
       <<",\"detachment_before\":"<<detachmentBefore<<",\"detachment_after\":"<<lifetime.detachments
       <<",\"scene_disabled\":"<<(!scene.IsEnabled()?"true":"false")
       <<",\"scene_unbound\":"<<(!scene.IsBound()?"true":"false")
       <<",\"scene_destroyed\":"<<lifetime.destroyed<<",\"font_requests\":"<<lifetime.fontRequests
       <<",\"cleanup_failed\":"<<(lifetime.cleanupFailed?"true":"false")
       <<",\"cleanup_before_gl4es_close\":true}"
       <<",\"execution_completed\":true,\"pixel_validation\":\"pending independent host analysis\",\"error\":\"\"}";
    return out.str();
}
}

