// CPU-only platform bring-up of the genuine graphics application. No structures,
// files or research workflows are loaded, and no GL context is manufactured.
#include "gxapp.h"
#include "gxfiles.h"
#include "glscene.h"
#include "olxstate.h"
#include "original_interpreter.h"
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
std::string output;
struct SceneCounts { unsigned make_current=0, destroyed=0, font_requests=0; };
class DisabledPlatformScene final: public AGlScene {
  SceneCounts& counts;
  TGlFont& DoCreateFont(TGlFont&, bool) const override {
    ++counts.font_requests;
    throw std::runtime_error("Font backend is not connected by the CPU lifecycle check");
  }
public:
  explicit DisabledPlatformScene(SceneCounts& c): counts(c) {}
  ~DisabledPlatformScene() override { ++counts.destroyed; }
  void ScaleFonts(double) override { throw std::runtime_error("No font backend"); }
  void RestoreFontScale() override { throw std::runtime_error("No font backend"); }
  olxstr ShowFontDialog(TGlFont*, const olxstr&) override { throw std::runtime_error("No font dialog"); }
  bool MakeCurrent() override { ++counts.make_current; return false; }
};
std::string Quote(const std::string& text) {
  std::ostringstream out; out << '"';
  for (unsigned char c: text) {
    if (c=='"' || c=='\\') out << '\\' << c;
    else if(c>=32) out << c;
    else out << "\\u00" << "0123456789abcdef"[c>>4] << "0123456789abcdef"[c&15];
  }
  out << '"'; return out.str();
}
olxstr Text(const char* s) { return olxstr::FromUTF8(s); }
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}
extern "C" const char* olex2_gx_lifecycle_run(const char* base) {
  try {
    olex2::harmony::OriginalInterpreterGuard originalScope;
    Require(base && base[0]=='/', "Absolute platform base directory required");
    Require(!TBasicApp::HasInstance(), "An original application is already alive");
    std::ostringstream out;
    out << "{\"scope\":\"original TGXApp empty CPU lifecycle; no GPU/font/UI or structure data\",\"cycles\":[";
    for (unsigned cycle=1; cycle<=3; ++cycle) {
      SceneCounts counts;
      std::unique_ptr<DisabledPlatformScene> scene(new DisabledPlatformScene(counts));
      std::unique_ptr<TGXApp> app(new TGXApp(Text(base), scene.get()));
      scene.release(); // The genuine TGlRenderer now owns the platform scene.
      const bool basic=&TBasicApp::GetInstance()==static_cast<TBasicApp*>(app.get());
      const bool core=&TXApp::GetInstance()==static_cast<TXApp*>(app.get());
      const bool graphics=&TGXApp::GetInstance()==app.get();
      const bool owned_scene=app->GetRenderer().GetScene().Parent()==&app->GetRenderer();
      const bool disabled=!app->GetRenderer().GetScene().IsEnabled();
      const size_t files=app->XFiles().Count();
      const bool gx_file=files==1 && dynamic_cast<TGXFile*>(&app->TXApp::XFile())!=nullptr;
      const bool no_loader=files==1 && !app->XFile().HasLastLoader();
      const size_t macros=app->GetLibrary().MacroCount(), functions=app->GetLibrary().FunctionCount();
      const bool mouse=app->GetLibrary().GetLibraryByName("mouse")!=nullptr;
      const bool state_identity=&TStateRegistry::GetInstance()==&app->GetStatesRegistry();
      const bool structure_visible=app->GetStatesRegistry().CheckState("strvis");
      const bool frame_visible=app->GetStatesRegistry().CheckState("wboxvis");
      const size_t fonts=app->GetRenderer().GetScene().FontCount();
      Require(basic && core && graphics && owned_scene && disabled, "Real application/renderer ownership differs");
      Require(gx_file && no_loader && macros>135 && mouse && state_identity, "Original graphics registration incomplete");
      Require(structure_visible && !frame_visible && fonts==0 && counts.font_requests==0, "Unexpected pre-surface scene state");
      app.reset();
      const bool singleton_cleared=!TBasicApp::HasInstance();
      Require(singleton_cleared && counts.destroyed==1 && counts.font_requests==0, "Original graphics lifecycle did not release scene/singleton");
      if(cycle>1) out << ',';
      out << "{\"cycle\":" << cycle << ",\"basic_identity\":" << basic
          << ",\"core_identity\":" << core << ",\"graphics_identity\":" << graphics
          << ",\"scene_owned_by_renderer\":" << owned_scene << ",\"scene_disabled\":" << disabled
          << ",\"file_count\":" << files << ",\"graphics_file_type\":" << gx_file
          << ",\"no_loaded_file\":" << no_loader << ",\"macro_count\":" << macros
          << ",\"function_count\":" << functions << ",\"mouse_library\":" << mouse
          << ",\"state_registry_identity\":" << state_identity
          << ",\"structure_visible\":" << structure_visible << ",\"frame_visible\":" << frame_visible
          << ",\"font_count\":" << fonts << ",\"font_requests\":" << counts.font_requests
          << ",\"make_current_attempts\":" << counts.make_current
          << ",\"scene_destroyed\":" << counts.destroyed
          << ",\"singleton_cleared\":" << singleton_cleared << '}';
    }
    out << "],\"execution_completed\":true}";
    output=out.str();
  } catch(const TExceptionBase& e) {
    output="{\"execution_completed\":false,\"error\":"+Quote(e.GetException()->GetFullMessage().ToMBStr().c_str())+"}";
  } catch(const std::exception& e) {
    output="{\"execution_completed\":false,\"error\":"+Quote(e.what())+"}";
  }
  return output.c_str();
}
