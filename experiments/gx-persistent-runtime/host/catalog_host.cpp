// Genuine GX registration and original metadata notifications, CPU only.
// No loaded structures, synthetic registry or GL/context mock implementation.
#include "gxapp.h"
#include "platform_window_scene.h"
#include "original_interpreter.h"
#include "original_log.h"
#include "eutf8.h"
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace olex2::harmony;
namespace {
unsigned assertions=0;
void Check(bool value,const char* message){++assertions;if(!value)throw std::runtime_error(message);}
std::string Utf8(const olxstr& value){const olxcstr bytes=TUtf8::Encode(value);return {bytes.c_str(),bytes.Length()};}
olxstr Text(const std::string& text){return TUtf8::Decode(olxcstr(text.data(),text.size()));}
std::string Hex(const std::string& text){const char* digits="0123456789abcdef";std::string out;for(unsigned char c:text){out+=digits[c>>4];out+=digits[c&15];}return out;}
}
int main(int argc,char** argv){try{
 Check(argc==2,"Expected an isolated absolute application path");
 OriginalInterpreterGuard guard;Check(!TBasicApp::HasInstance(),"Existing original singleton");
 PlatformSceneLifetime lifetime;
 {
  auto scene=std::make_unique<PlatformWindowScene>(lifetime);
  auto app=std::make_unique<TGXApp>(Text(argv[1]),scene.get());scene.release();
  Check(&TBasicApp::GetInstance()==static_cast<TBasicApp*>(app.get()),"Basic owner identity differs");
  Check(&TXApp::GetInstance()==static_cast<TXApp*>(app.get()) && &TGXApp::GetInstance()==app.get(),"Core/GX owner identity differs");
  Check(!app->GetRenderer().GetScene().IsEnabled() && app->GetRenderer().GetScene().Parent()==&app->GetRenderer(),"Original renderer does not own disabled platform scene");
  Check(app->XFiles().Count()==1 && !app->XFile().HasLastLoader(),"Unexpected nonempty application");
  Check(app->GetLibrary().MacroCount()==193 && app->GetLibrary().FunctionCount()==46,"Unexpected actual GX registry");
  OriginalLogCapture capture(app->GetLog());
  Check(capture.Snapshot().posted==0,"Fresh original log observer is not empty");
  for(const char* kind:{"macros","functions"}){
   const bool macros=std::string(kind)=="macros";capture.Clear();const auto before=capture.Snapshot();
   TBasicFunctionPList entries;if(macros)app->GetLibrary().ListAllMacros(entries);else app->GetLibrary().ListAllFunctions(entries);
   OriginalProcessor processor;TLibrary* exposed=processor.GetLibrary().AddLibrary("core");
   for(const char* name:{"LstMac","LstFun"}){
    auto* original=app->GetLibrary().FindMacro(name);Check(original!=nullptr,"Missing genuine metadata macro");
    std::unique_ptr<ABasicFunction> copy(original->Replicate());auto* clone=dynamic_cast<AMacro*>(copy.get());Check(clone!=nullptr,"Original metadata clone failed");
    exposed->Register(clone);copy.release();
   }
   Check(exposed->MacroCount()==2,"Full registry accidentally exposed");TMacroData error;
   const bool passed=processor.processMacroEx(Text(macros?"core.LstMac":"core.LstFun"),error,"HarmonyPC independent genuine GX metadata host",true);
   Check(passed,"Original metadata dispatch failed");const auto observed=capture.Snapshot();
   std::cerr<<"GX_CATALOG_OBSERVATION "<<kind<<" root="<<(macros?app->GetLibrary().MacroCount():app->GetLibrary().FunctionCount())<<" recursive="<<entries.Count()<<" entries="<<observed.entries.size()<<" posted="<<observed.posted-before.posted<<" dropped="<<observed.dropped<<" bytes="<<observed.retainedBytes<<" truncated="<<observed.truncated<<"\n";
   Check(entries.Count()==(macros?196:52) && observed.entries.size()==entries.Count(),"Original recursive metadata count differs");
   Check(observed.posted-before.posted==entries.Count() && observed.attached && observed.captureFailures==0 && observed.dropped==0 && observed.truncated==0,"Original metadata notifications lost or changed");
   size_t bytes=0;
   for(size_t index=0;index<entries.Count();++index){
    const auto expected=Utf8(entries[index]->GetQualifiedName())+" - "+Utf8(entries[index]->GetSignature())+"\n";
    Check(observed.entries[index].text==expected && !observed.entries[index].truncated,"Original qualified signature/log bytes differ");
    bytes+=expected.size();std::cout<<"GX_CATALOG_LINE "<<kind<<' '<<index<<' '<<Hex(expected)<<'\n';
   }
   Check(bytes==observed.retainedBytes,"Exact captured metadata byte accounting differs");
   std::cout<<"GX_CATALOG_DIGEST "<<kind<<' '<<entries.Count()<<' '<<observed.retainedBytes<<' '<<observed.textSha256<<'\n';
  }
  Check(!app->XFile().HasLastLoader() && app->GetRenderer().GetScene().FontCount()==0 && lifetime.bindings==0 && lifetime.fontRequests==0,"Metadata query changed empty application or used fonts/GL");
 }
 Check(!TBasicApp::HasInstance() && lifetime.destroyed==1 && lifetime.bindings==0 && lifetime.detachments==0,"Original GX owner/scene release differs");
 std::cout<<"PASS genuine GX metadata host; "<<assertions<<" assertions; root=193/46; recursive=196/52; no loaded data, GL context, fonts or full application qualification\n";return 0;
}catch(const TExceptionBase& e){std::cerr<<Utf8(e.GetException()->GetFullMessage())<<'\n';return 1;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
