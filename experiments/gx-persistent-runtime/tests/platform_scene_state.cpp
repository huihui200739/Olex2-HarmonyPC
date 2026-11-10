#include "platform_window_scene.h"
#include <iostream>
#include <stdexcept>
using namespace olex2::harmony;
namespace {
unsigned assertions=0,calls=0;
void Check(bool ok){++assertions;if(!ok)throw std::runtime_error("Scene state assertion failed at "+std::to_string(assertions));}
int Current(){++calls;return 1;}
int Fails(){++calls;return 0;}
void Size(int* w,int* h){*w=*h=100;}
void* Resolve(const char*){return nullptr;}
template<class F> void Rejected(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}Check(failed);}
}
int main(){
 PlatformSceneLifetime counts;
 {
  PlatformWindowScene scene(counts);Check(!scene.IsEnabled());Check(!scene.IsBound());Check(!scene.MakeCurrent());Check(calls==0);Check(counts.makeCurrent==1);
  Olex2LegacyHost host{};
  Rejected([&]{scene.Bind(host);});Check(counts.bindings==0);Check(!scene.IsBound());
  host.make_current=Current;Rejected([&]{scene.Bind(host);});host.framebuffer_size=Size;Rejected([&]{scene.Bind(host);});host.resolve=Resolve;
  for(unsigned cycle=1;cycle<=3;++cycle){
   scene.Bind(host);Check(scene.IsEnabled());Check(scene.IsBound());Check(counts.bindings==cycle);Check(scene.MakeCurrent());Check(calls==cycle*3-2);
   Rejected([&]{scene.Bind(host);});Check(counts.bindings==cycle);Check(scene.IsBound());
   host.make_current=Fails;Check(scene.MakeCurrent());host.make_current=Current; // The ABI was copied, not borrowed.
   Check(scene.StartDraw());scene.EndDraw();Check(counts.startDraw==cycle);Check(counts.endDraw==cycle);
   scene.SetEnabled(false);Check(!scene.MakeCurrent());Check(calls==cycle*3);scene.SetEnabled(true);
   scene.Unbind();Check(!scene.IsEnabled());Check(!scene.IsBound());Check(counts.detachments==cycle);Check(!scene.MakeCurrent());Check(calls==cycle*3);
   scene.Unbind();Check(counts.detachments==cycle);Check(counts.destroyed==0);
  }
  Check(counts.fontRequests==0);Rejected([&]{scene.ScaleFonts(2);});Rejected([&]{scene.RestoreFontScale();});Rejected([&]{scene.ShowFontDialog(nullptr,"");});Check(counts.fontRequests==0);
  Rejected([&]{scene.RequestModeledFont();});Check(counts.fontRequests==1);
 }
 Check(counts.destroyed==1);Check(counts.bindings==counts.detachments);Check(!counts.cleanupFailed);
 std::cout<<"PASS platform scene state machine; "<<assertions<<" assertions; modeled base, no GL or original-scene qualification\n";
}
