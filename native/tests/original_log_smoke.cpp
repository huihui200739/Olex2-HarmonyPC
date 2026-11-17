#include "application_runtime.h"
#include "original_interpreter.h"
#include "eutf8.h"
#include "xapp.h"
#include <atomic>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <thread>
#include <unistd.h>
using namespace olex2::harmony;
namespace {
unsigned assertions=0;
void Check(bool ok,const char* message){++assertions;if(!ok)throw std::runtime_error(message);}
olxstr Text(const std::string& s){return TUtf8::Decode(olxcstr(s.data(),s.size()));}
std::string Utf8(const olxstr& s){const olxcstr bytes=TUtf8::Encode(s);return std::string(bytes.c_str(),bytes.Length());}
std::string Hex(const std::string& s){const char* digits="0123456789abcdef";std::string out;for(unsigned char c:s){out+=digits[c>>4];out+=digits[c&15];}return out;}
void Invariants(const OriginalLogSnapshot& s){
 Check(s.attached && s.entries.size()<=MaximumOriginalLogEntries && s.retainedBytes<=MaximumOriginalLogBytes && s.captureFailures==0,"Original log capture is not bounded/attached or lost a notification");
 Check(s.posted==s.dropped+s.clearedEntries+s.entries.size(),"Original notification accounting differs");
 size_t bytes=0;uint64_t prior=0;
 for(const auto& e:s.entries){Check(e.sequence>prior && e.sequence<=s.posted && e.text.size()<=MaximumOriginalLogEntryBytes,"Original log entry identity/budget differs");Check(Utf8(Text(e.text))==e.text,"Truncation split or changed canonical UTF-8");prior=e.sequence;bytes+=e.text.size();}
 Check(bytes==s.retainedBytes && s.textSha256.size()==64,"Original byte accounting/digest differs");
}
struct Counter:public AActionHandler{unsigned count=0;Counter(){SetToDelete(false);}bool Execute(const IOlxObject*,const IOlxObject*,TActionQueue*)override{++count;return false;}};
struct OwnedDirectory {
 std::string root;
 explicit OwnedDirectory(const char* parent){std::string pattern=std::string(parent)+"/olex2-log-XXXXXX";std::vector<char> b(pattern.begin(),pattern.end());b.push_back(0);char* p=mkdtemp(b.data());if(!p)throw std::runtime_error("Cannot create isolated original-log runtime fixture");root=p;}
 ~OwnedDirectory(){const std::string p=root+"/olex2-runtime-v1";unlink((p+"/base/OLEX2-LICENCE.txt").c_str());unlink((p+"/config/.options").c_str());for(const char* d:{"base","shared","instance","config"})rmdir((p+"/"+d).c_str());rmdir(p.c_str());rmdir(root.c_str());}
};
std::string Read(const char* p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Cannot read original licence");return {std::istreambuf_iterator<char>(f),{}};}
}
int main(int argc,char** argv){try{
 if(argc!=3)throw std::invalid_argument("Expected own fixture parent and exact original licence");
 unsigned originalPostCases=0;
 TLog source;Counter other;source.OnPost.Add(&other);const auto beforeHandlers=source.OnPost.HandlerCount();
 {
  OriginalLogCapture capture(source);Check(source.OnPost.HandlerCount()==beforeHandlers+1,"Original observer did not attach");auto s=capture.Snapshot();Invariants(s);Check(s.entries.empty() && s.posted==0 && s.textSha256=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","New observer retained old state or wrong empty digest");
  for(Logging level:{logDefault,logInfo,logWarning,logError,logException}){source.NewEntry(level)<<Text("synthetic 原版日志");++originalPostCases;}
  s=capture.Snapshot();Invariants(s);Check(s.posted==5 && other.count==5,"Severity notifications were duplicated or another observer was suppressed");
  for(const auto& e:s.entries)Check(e.text=="synthetic 原版日志\n" && !e.truncated,"Original OnPost text/newline was changed");
  source.NewEntry(logVerbose)<<"synthetic hidden";Check(capture.Snapshot().posted==5,"Original verbose filtering was bypassed");
  source.SetVerbose(true);source.NewEntry(logVerbose)<<"synthetic visible";Check(capture.Snapshot().posted==6,"Original verbose notification was not captured");
  source<<olxstr("synthetic direct stream");Check(capture.Snapshot().posted==6,"Direct output was falsely labelled an OnPost notification");
  {auto disabled=source.Disable();source.NewEntry()<<"synthetic disabled post";}
  Check(capture.Snapshot().posted==7,"Original OnPost-disabled-stream behavior was rewritten");
  source.NewEntry();s=capture.Snapshot();Check(s.entries.back().text=="\n","Empty original notification was suppressed");
  const std::string nul("a\0b",3);source.NewEntry()<<Text(nul);s=capture.Snapshot();Check(s.entries.back().text==nul+"\n","Original embedded NUL bytes were lost");
  const auto posted=s.posted;capture.Clear();s=capture.Snapshot();Invariants(s);Check(s.entries.empty() && s.posted==posted && s.clearedEntries==posted && s.clearCount==1,"Clear reset sequence accounting or retained text");
  const std::string crossing=std::string(4095,'a')+"汉尾";source.NewEntry()<<Text(crossing);s=capture.Snapshot();Invariants(s);Check(s.entries.size()==1 && s.entries[0].text==std::string(4095,'a') && s.entries[0].truncated && s.truncated==1,"UTF-8 boundary truncation differs");
  capture.Clear();for(unsigned i=0;i<300;++i)source.NewEntry()<<Text("synthetic "+std::to_string(i));s=capture.Snapshot();Invariants(s);Check(s.entries.size()==256 && s.dropped==44,"Entry-count eviction differs");
  capture.Clear();for(unsigned i=0;i<20;++i)source.NewEntry()<<Text(std::string(4095,'x'));s=capture.Snapshot();Invariants(s);Check(s.entries.size()==16 && s.retainedBytes==65536 && s.dropped==48,"UTF-8 byte-budget eviction differs");
  capture.Clear();std::atomic<bool> done{false};std::atomic<unsigned> snapshots{0};std::thread reader([&]{while(!done){OriginalInterpreterGuard guard;auto value=capture.Snapshot();if(value.retainedBytes>MaximumOriginalLogBytes)std::terminate();++snapshots;}});
  while(snapshots.load()==0)std::this_thread::yield();
  std::vector<std::thread> workers;for(unsigned worker=0;worker<4;++worker)workers.emplace_back([&,worker]{for(unsigned i=0;i<100;++i){OriginalInterpreterGuard guard;source.NewEntry()<<Text("synthetic thread "+std::to_string(worker)+":"+std::to_string(i));}});
  for(auto& worker:workers)worker.join();done=true;reader.join();s=capture.Snapshot();Invariants(s);Check(snapshots>0 && s.entries.size()==256 && s.entries.back().sequence==s.posted,"Concurrent original posting/snapshot lost its bounded stream");
 }
 const auto prior=other.count;source.NewEntry()<<"synthetic after detach";Check(source.OnPost.HandlerCount()==beforeHandlers && other.count==prior+1,"Observer detached incorrectly or changed other handlers");source.OnPost.Remove(&other);
 unsigned catalogs=0;
 {
  OwnedDirectory owned(argv[1]);ApplicationRuntime runtime(owned.root,Read(argv[2]));auto s=runtime.OriginalLogs();Invariants(s);Check(s.posted==0 && s.entries.empty(),"New runtime exposed a previous capture");
  for(const char* kind:{"macros","functions"}){
   const bool macros=std::string(kind)=="macros";runtime.ClearOriginalLogs();const auto start=runtime.OriginalLogs();const auto result=runtime.RunCoreCatalog(macros?"core.LstMac":"core.LstFun");s=runtime.OriginalLogs();Invariants(s);
   Check(result.passed && result.error.empty() && result.exposedMacros==2,"Actual original catalog macro did not dispatch through its bounded library");
   TBasicFunctionPList entries;if(macros)TXApp::GetInstance().GetLibrary().ListAllMacros(entries);else TXApp::GetInstance().GetLibrary().ListAllFunctions(entries);
   Check(entries.Count()==(macros?135:38) && s.entries.size()==entries.Count() && result.postedAfter-result.postedBefore==entries.Count() && result.postedBefore==start.posted,"Original catalog produced incorrect actual notification count");
   for(size_t i=0;i<entries.Count();++i){const std::string expected=Utf8(entries[i]->GetQualifiedName())+" - "+Utf8(entries[i]->GetSignature())+"\n";Check(s.entries[i].text==expected && !s.entries[i].truncated,"Original catalog logs differ from original registered metadata");std::cout<<"CATALOG_LINE "<<kind<<" "<<i<<" "<<Hex(s.entries[i].text)<<"\n";}
   std::cout<<"CATALOG_DIGEST "<<kind<<" "<<s.entries.size()<<" "<<s.retainedBytes<<" "<<s.textSha256<<"\n";++catalogs;
  }
  const auto before=runtime.OriginalLogs();for(const std::string input:std::vector<std::string>{"core.Unknown","LstMac","core.Run","core.LstFS","app.Platform()",std::string(2049,'a'),std::string("core.LstMac\0x",13),std::string("\xff",1)}){const auto r=runtime.RunCoreCatalog(input);if(r.passed || runtime.OriginalLogs().posted!=before.posted)std::cerr<<"UNEXPECTED_CATALOG_INPUT "<<Hex(input)<<" passed="<<r.passed<<" posted="<<r.postedAfter-r.postedBefore<<"\n";Check(!r.passed && !r.error.empty() && runtime.OriginalLogs().posted==before.posted,"Unsupported catalog input executed or changed logs");}
  auto accepted=runtime.RunCoreCatalog("core.LstMac()");Check(accepted.passed && accepted.postedAfter-accepted.postedBefore==135,"Original parenthesized macro syntax was not preserved");
  auto r=runtime.RunCoreCatalog("core.LstMac -h >> core.Unknown");Check(!r.passed && !r.error.empty() && r.postedAfter>r.postedBefore,"Original failed-chain notifications were falsely rolled back");
  const auto state=runtime.Status();Check(state.originalCoreEmptyFile && state.originalCoreMacroCount==135 && state.originalCoreFunctionCount==38 && state.optionsCount==0,"Catalog query changed original core/options state");
 }
 Check(!TBasicApp::HasInstance(),"Original singleton survived capture destruction");
 std::cout<<"PASS original log bridge; "<<assertions<<" assertions; "<<originalPostCases<<" original severity cases; "<<catalogs<<" real core catalog macros; 400 locally serialized caller posts; bounded original OnPost only, not full command behavior or durable logging\n";return 0;
}catch(const TExceptionBase& e){std::cerr<<Utf8(e.GetException()->GetFullMessage())<<"\n";return 1;}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
