// Generic synthetic view state only; no user files or research data.
#include "desktop_commands.h"
#include "integration.h"
#include <cmath>
#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <stdexcept>
using namespace olex2::harmony;
namespace {unsigned assertions=0;
void Check(bool value,const char* message) {if(!value)throw std::runtime_error(message);++assertions;}
}
int main() {
 try {
  const DesktopCommandState initial;
  for(const auto& item:{std::pair<const char*,unsigned>{"sphere",0},{"torus",1},{"disks",2},{"rims",3}}) {
    const auto result=ExecuteDesktopCommand(std::string("pc.mesh ")+item.first);
    Check(result.passed && result.dispatched==1 && result.state.primitive==item.second,"original qualified macro lookup failed");
  }
  auto quoted=ExecuteDesktopCommand("PC.MESH 'torus'");Check(quoted.passed && quoted.state.primitive==1,"original quoted/case-insensitive macro parsing failed");
  for(unsigned i=0;i<=5;++i) {const auto r=ExecuteDesktopCommand("pc.detail "+std::to_string(i));Check(r.passed && r.state.generation==i,"detail adapter failed");}
  for(const auto& item:{std::pair<const char*,unsigned>{"jade",0},{"amber",1},{"sky",2}}) {const auto r=ExecuteDesktopCommand(std::string("pc.color ")+item.first);Check(r.passed && r.state.colorPreset==item.second,"color adapter failed");}
  for(const auto& item:{std::pair<const char*,unsigned>{"classic",0},{"soft",1},{"flat",2},{"glossy",3}}) {const auto r=ExecuteDesktopCommand(std::string("pc.material ")+item.first);Check(r.passed && r.state.materialPreset==item.second,"material adapter failed");}
  auto on=ExecuteDesktopCommand("pc.wire on");Check(on.passed && on.state.wireframe,"wire on failed");
  auto toggle=ExecuteDesktopCommand("pc.wire toggle",on.state);Check(toggle.passed && !toggle.state.wireframe,"wire toggle failed");
  auto view=ExecuteDesktopCommand("pc.view 41 -18 4.8");Check(view.passed && view.state.yaw==41 && view.state.pitch==-18 && std::abs(view.state.distance-4.8f)<1.e-6,"original negative numeric arguments failed");
  auto reset=ExecuteDesktopCommand("pc.reset",view.state);Check(reset.passed && SameDesktopCommandState(reset.state,initial),"reset does not preserve initial view semantics");
  auto help=ExecuteDesktopCommand("pc.help");Check(help.passed && !help.changed && help.text.find("not the complete Olex2")!=std::string::npos,"help overstates command integration");
  auto sequence=ExecuteDesktopCommand("pc.mesh torus >> pc.detail 2 >> pc.wire on");Check(sequence.passed && sequence.dispatched==3 && sequence.state.primitive==1 && sequence.state.generation==2 && sequence.state.wireframe,"original chained-command parser failed");
  for(const auto& text:{"pc.missing","pc.detail","pc.detail 9","pc.detail 2.5","pc.mesh missing","pc.wire yes","pc.color missing","pc.material missing","pc.view 0 86 4","pc.view 0 0 12","pc.help extra","pc.detail 2 >> pc.mesh missing","   "}) {
    auto r=ExecuteDesktopCommand(text);Check(!r.passed && !r.changed && !r.error.empty() && SameDesktopCommandState(r.state,initial),"invalid command mutated transaction state or appeared successful");
  }
  const char* meshes[]={"sphere","torus","disks","rims"};const char* colors[]={"jade","amber","sky"};const char* materials[]={"classic","soft","flat","glossy"};unsigned combinations=0;
  for(unsigned mesh=0;mesh<4;++mesh)for(unsigned detail=0;detail<6;++detail)for(unsigned color=0;color<3;++color)for(unsigned material=0;material<4;++material)for(bool wire:{false,true}) {
    const std::string command=std::string("pc.mesh ")+meshes[mesh]+" >> pc.detail "+std::to_string(detail)+" >> pc.color "+colors[color]+" >> pc.material "+materials[material]+" >> pc.wire "+(wire?"on":"off");
    const auto r=ExecuteDesktopCommand(command,view.state);DesktopCommandState expected=view.state;expected.primitive=mesh;expected.generation=detail;expected.colorPreset=color;expected.materialPreset=material;expected.wireframe=wire;
    Check(r.passed && r.dispatched==5 && SameDesktopCommandState(r.state,expected),"independent view combination does not match original dispatch result");++combinations;
    const auto invalid=ExecuteDesktopCommand("pc.reset >> pc.detail 9",r.state);
    Check(!invalid.passed && !invalid.changed && SameDesktopCommandState(invalid.state,r.state),"failed command chain did not roll back non-default initial view");
  }
  auto wrap=ExecuteDesktopCommand("pc.view -800 85 10");Check(wrap.passed && wrap.state.yaw==-80 && wrap.state.pitch==85 && wrap.state.distance==10,"finite view boundary or original negative argument parsing failed");
  auto same=ExecuteDesktopCommand("pc.mesh sphere");Check(same.passed && !same.changed,"no-op command incorrectly changes state");
  auto unknown=ExecuteDesktopCommand(u8"未接通命令");Check(!unknown.passed && !unknown.error.empty(),"unsupported Unicode command was accepted");
  std::string within,excess;
  for(unsigned i=0;i<32;++i){if(i)within+=" >> ";within+="pc.wire toggle";}
  excess=within+" >> pc.reset";
  auto full=ExecuteDesktopCommand(within);Check(full.passed && full.dispatched==32 && !full.state.wireframe,"bounded command sequence failed");
  auto over=ExecuteDesktopCommand(excess,view.state);Check(!over.passed && over.dispatched==32 && SameDesktopCommandState(over.state,view.state),"explicit adapter dispatch limit was not transactional");
  std::atomic<unsigned> failures{0};std::vector<std::thread> threads;
  for(unsigned i=0;i<4;++i)threads.emplace_back([&,i]{for(unsigned j=0;j<16;++j){auto r=ExecuteDesktopCommand(std::string("pc.mesh ")+meshes[i]);if(!r.passed || r.state.primitive!=i)++failures;}});
  for(auto& t:threads)t.join();Check(failures==0,"serialized adapter invocations interfered across callers");
  Check(olex2::IOlex2Processor::GetInstance()==nullptr,"original current-processor pointer was not restored");
  std::cout<<"PASS original desktop command interpreter; "<<assertions<<" assertions; "<<combinations<<" independent view combinations; 64 serialized concurrent calls; original TEMacroLib/TLibrary dispatch; explicit pc.* adapters only, not full application\n";
 }catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
