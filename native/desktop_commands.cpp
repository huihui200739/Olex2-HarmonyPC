#include "desktop_commands.h"
#include "original_interpreter.h"
#include "eutf8.h"
#include <algorithm>
#include <cmath>
#include <locale>
#include <mutex>
#include <sstream>
#include <stdexcept>
namespace olex2::harmony {
bool SameDesktopCommandState(const DesktopCommandState& a,const DesktopCommandState& b) {
  return a.primitive==b.primitive && a.generation==b.generation && a.colorPreset==b.colorPreset && a.materialPreset==b.materialPreset && a.wireframe==b.wireframe && a.yaw==b.yaw && a.pitch==b.pitch && a.distance==b.distance;
}
namespace {
std::string Utf8(const olxstr& text) {return TUtf8::Encode(text).c_str();}
void Validate(const DesktopCommandState& s) {
  if(s.primitive>3 || s.generation>5 || s.colorPreset>2 || s.materialPreset>3 || !std::isfinite(s.yaw) || !std::isfinite(s.pitch) || !std::isfinite(s.distance) || s.pitch<-85 || s.pitch>85 || s.distance<2.2f || s.distance>10)
    throw std::invalid_argument("Invalid initial desktop view state");
}
double Number(const olxstr& text) {
  std::istringstream stream(Utf8(text));stream.imbue(std::locale::classic());double value=0;
  stream>>std::noskipws>>value;
  if(!stream || !stream.eof() || !std::isfinite(value) || std::abs(value)>1.e6)
    throw std::invalid_argument("Expected a finite bounded number");
  return value;
}
unsigned Choice(const olxstr& text,const std::initializer_list<const char*>& names) {
  unsigned i=0;for(const char* name:names) {if(text.Equalsi(name))return i;++i;}
  throw std::invalid_argument("Unsupported value for this pc.* command; use pc.help");
}
class Processor final:public OriginalProcessor {
  DesktopCommandResult& result_;
  void Before() {if(result_.dispatched>=32)throw std::length_error("Desktop command dispatch budget exceeded");++result_.dispatched;}
  template<class F>void Apply(TMacroData& error,F action) {
    Before();
    try {action();error.SetRetVal(olxstr("ok"));}
    catch(const std::exception& e) {error.ProcessingError("HarmonyPC pc.* adapter",TUtf8::Decode(e.what()));}
  }
  void Mesh(TStrObjList& p,const TParamList&,TMacroData& e) {Apply(e,[&]{result_.state.primitive=Choice(p[0],{"sphere","torus","disks","rims"});});}
  void Detail(TStrObjList& p,const TParamList&,TMacroData& e) {Apply(e,[&]{const double v=Number(p[0]);if(v<0 || v>5 || v!=std::floor(v))throw std::invalid_argument("Detail must be an integer 0..5");result_.state.generation=static_cast<unsigned>(v);});}
  void Wire(TStrObjList& p,const TParamList&,TMacroData& e) {Apply(e,[&]{const unsigned v=Choice(p[0],{"off","on","toggle"});result_.state.wireframe=v==2?!result_.state.wireframe:v==1;});}
  void Color(TStrObjList& p,const TParamList&,TMacroData& e) {Apply(e,[&]{result_.state.colorPreset=Choice(p[0],{"jade","amber","sky"});});}
  void Material(TStrObjList& p,const TParamList&,TMacroData& e) {Apply(e,[&]{result_.state.materialPreset=Choice(p[0],{"classic","soft","flat","glossy"});});}
  void View(TStrObjList& p,const TParamList&,TMacroData& e) {Apply(e,[&]{
    const double yaw=Number(p[0]),pitch=Number(p[1]),distance=Number(p[2]);
    if(pitch<-85 || pitch>85 || distance<2.2f || distance>10)throw std::invalid_argument("View pitch is -85..85; distance is 2.2..10");
    result_.state.yaw=static_cast<float>(std::fmod(yaw,360.));result_.state.pitch=static_cast<float>(pitch);result_.state.distance=static_cast<float>(distance);
  });}
  void Reset(TStrObjList&,const TParamList&,TMacroData& e) {Apply(e,[&]{result_.state.yaw=32;result_.state.pitch=-22;result_.state.distance=4.4f;});}
  void Help(TStrObjList&,const TParamList&,TMacroData& e) {
    Apply(e,[]{});result_.text="pc.mesh sphere|torus|disks|rims; pc.detail 0..5; pc.wire off|on|toggle; pc.color jade|amber|sky; pc.material classic|soft|flat|glossy; pc.view yaw pitch distance; pc.reset; pc.help. New platform view adapters only, not the complete Olex2 command library.";
    e.SetRetVal(TUtf8::Decode(result_.text.c_str()));
  }
  template<class Method>void Add(TLibrary& lib,const char* name,Method method,uint32_t args) {
    lib.Register(new TMacro<Processor>(this,method,name,EmptyString(),args,"HarmonyPC explicit view adapter"));
  }
public:
  explicit Processor(DesktopCommandResult& result):result_(result) {
    // No TBasicApp singleton, filesystem log or environment settings are
    // created/changed. Do not expose Init's LogLevel mutation in this adapter.
    TLibrary* pc=library_.AddLibrary("pc");
    Add(*pc,"mesh",&Processor::Mesh,fpOne);Add(*pc,"detail",&Processor::Detail,fpOne);
    Add(*pc,"wire",&Processor::Wire,fpOne);Add(*pc,"color",&Processor::Color,fpOne);
    Add(*pc,"material",&Processor::Material,fpOne);Add(*pc,"view",&Processor::View,fpThree);
    Add(*pc,"reset",&Processor::Reset,fpNone);Add(*pc,"help",&Processor::Help,fpNone);
  }
};
}

DesktopCommandResult ExecuteDesktopCommand(const std::string& text,const DesktopCommandState& initial) {
  DesktopCommandResult result;result.state=initial;
  // The upstream dispatcher uses a process-wide current-processor pointer.
  // Serialize this adapter and restore any previous pointer on every exit.
  OriginalInterpreterGuard interpreter_guard;
  try {
    Validate(initial);
    if(text.empty() || text.size()>2048 || text.find('\0')!=std::string::npos)throw std::invalid_argument("Desktop command must contain 1..2048 UTF-8 bytes without NUL");
    Processor processor(result);TMacroData error;
    processor.processMacroEx(TUtf8::Decode(text.c_str()),error,"HarmonyPC desktop command",true);
    if(!error.IsSuccessful())result.error=Utf8(error.GetInfo());
    else if(result.dispatched==0)result.error="No registered pc.* command was dispatched";
    else {result.passed=true;result.changed=!SameDesktopCommandState(initial,result.state);if(result.text.empty())result.text="pc.* command accepted by original OlexSys parser and dispatcher";}
  }
  catch(const TExceptionBase& e) {result.error=Utf8(e.GetException()->GetFullMessage());}
  catch(const std::exception& e) {result.error=e.what();}
  if(!result.passed) {result.state=initial;result.changed=false;result.text.clear();if(result.error.empty())result.error="Desktop command was not accepted";}
  return result;
}
}
