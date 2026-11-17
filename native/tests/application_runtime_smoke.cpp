// Only an exclusively owned synthetic settings tree and the original licence.
#include "application_runtime.h"
#include "desktop_commands.h"
#include "xapp.h"
#include "integration.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>
using namespace olex2::harmony;
namespace {
unsigned assertions=0;
void Check(bool b,const char* message){if(!b)throw std::runtime_error(message);++assertions;}
std::string Read(const std::string& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Cannot read own runtime fixture");return {std::istreambuf_iterator<char>(f),{}};}
void Write(const std::string& p,const std::string& b){std::ofstream f(p,std::ios::binary);f<<b;if(!f)throw std::runtime_error("Cannot write own runtime fixture");}
std::string Eval(ApplicationRuntime& runtime,const std::string& expression){auto r=runtime.Evaluate(expression);if(!r.passed)throw std::runtime_error(expression+": "+r.error);return r.text;}
struct OwnedDirectory {
 std::string root;
 explicit OwnedDirectory(const std::string& parent){std::string p=parent+"/olex2-app-runtime-XXXXXX";std::vector<char>b(p.begin(),p.end());b.push_back(0);char* d=mkdtemp(b.data());if(!d)throw std::runtime_error("Cannot create own runtime fixture");root=d;}
 ~OwnedDirectory(){const auto runtime=root+"/olex2-runtime-v1";for(auto f:{"config/.options","config/.options.saved","base/OLEX2-LICENCE.txt"})unlink((runtime+"/"+f).c_str());for(auto f:{"config","instance","shared","base"})rmdir((runtime+"/"+f).c_str());rmdir(runtime.c_str());rmdir(root.c_str());}
};
}
int main(int argc,char** argv){
 try{
  if(argc!=3)throw std::invalid_argument("Expected private test parent and original licence file");
  const std::string license=Read(argv[2]),path=getenv("PATH")?getenv("PATH"):"";char cwd[4096];if(!getcwd(cwd,sizeof(cwd)))throw std::runtime_error("Cannot inspect working directory");
  OwnedDirectory owned(argv[1]);const auto root=owned.root+"/olex2-runtime-v1",config=root+"/config",options=config+"/.options";
  mkdir(root.c_str(),0700);mkdir(config.c_str(),0700);Write(options,"# isolated synthetic settings; preserve this comment\nui.commandHints=true\nuntouched.key=keep\nPATH=synthetic-unapplied\n");
  TEHashTreeMapEx<olxstr,int,olxstrComparator<true>> map;
  Check(map.Count()==0,"new original hashed registry count is not zero");map.Add("first",1);map.Add("second",2);Check(map.Count()==2,"original hashed registry count differs from inserted entries");map.Clear();Check(map.Count()==0,"cleared original hashed registry count is stale");map.Add("third",3);Check(map.Count()==1 && map.Find(olxstr("third"))!=nullptr,"reused original hashed registry count is incorrect");
  std::string saved;unsigned combinations=0;
  {
   ApplicationRuntime runtime(owned.root,license);auto s=runtime.Status();
   Check(s.originalCoreInitialized && s.originalCoreEmptyFile && s.originalCoreFileCount==1,"original TXApp did not create exactly one empty core container");
   Check(s.originalCoreMacroCount==135 && s.originalCoreFunctionCount==38,"original core registry count changed");
   Check(s.originalFileFormatCount==15,"Harmony runtime did not register all original desktop file-format aliases");
   Check(&TXApp::GetInstance()==dynamic_cast<TXApp*>(&TBasicApp::GetInstance()),"runtime is not the genuine original core singleton");
   Check(s.initialized && s.optionsRestored && s.licenseVerified && s.originalFunctionCount==22,"actual original runtime/registry/licence did not initialize");
   Check(s.optionsCount==3 && s.commandHints && s.saveRevision==0 && !s.lastSaveCommitted,"original options were not restored accurately");
   Check(s.licenseSha256==OriginalLicenseSha256() && Read(root+"/base/OLEX2-LICENCE.txt")==license,"resource mapping did not preserve exact original licence bytes");
   const auto base=Eval(runtime,"app.BaseDir()"),instance=Eval(runtime,"app.InstanceDir()"),shared=Eval(runtime,"app.SharedDir()"),conf=Eval(runtime,"app.ConfigDir()");
   Check(base==root+"/base" && instance==root+"/instance" && shared==root+"/shared" && conf==config,"original app directory functions do not use actual private paths");
   Check(Eval(runtime,"app.ArgCount()") == "1" && Eval(runtime,"app.GetArg(0)")=="olex2-harmony","original app argument registry not called");
   Check(Eval(runtime,"app.OptValue(untouched.key)")=="keep","original option lookup not called");
   Check(Eval(runtime,"app.GetLogName()").empty(),"runtime unexpectedly opened a desktop file log");
   const auto platform=Eval(runtime,"app.Platform()");Check(platform==s.platform,"original Platform did not dispatch through platform override");
   Eval(runtime,"app.SetOption(ui.commandHints,false)");s=runtime.Status();Check(!s.commandHints && s.saveRevision==1 && s.lastSaveCommitted,"original SetOption did not invoke actual atomic save override");
   saved=Read(options);Check(saved.find("# isolated synthetic settings; preserve this comment")!=std::string::npos && saved.find("untouched.key=keep")!=std::string::npos && saved.find("ui.commandHints=false")!=std::string::npos,"atomic original format save lost comments/options");
   Check(Eval(runtime,"app.OptValue(ui.commandHints)")=="false","original saved option return differs from real file");
   Check(Eval(runtime,"form.HasGUI()") == "true", "Harmony form GUI capability function differs");
   Check(Eval(runtime,"form.IsOS(harmony)") == "true" && Eval(runtime,"form.IsOS(ohos)") == "true" && Eval(runtime,"form.IsOS(linux)") == "false", "Harmony form OS function differs");
   Check(Eval(runtime,"form.Strcat(left,right)") == "leftright", "original form Strcat behavior differs");
   Check(Eval(runtime,"form.Strcmp(same,same)") == "true" && Eval(runtime,"form.Strcmp(left,right)") == "false", "original form Strcmp behavior differs");
   Check(Eval(runtime,"form.RGB(1,2,3)") == "197121" && Eval(runtime,"form.RGB(1,2,3,4)") == "67305985", "original form RGB packing differs");
   setenv("OLEX2_HARMONY_FORM_TEST", "preserved-value", 1);
   Check(Eval(runtime,"form.GetEnv(OLEX2_HARMONY_FORM_TEST)") == "preserved-value", "original form GetEnv behavior differs");
   unsetenv("OLEX2_HARMONY_FORM_TEST");
   for(const auto& expression:{"app.Unknown()","app.SetOption(ui.commandHints,sometimes)","app.SetOption('bad key',value)","app.SetOption(ui.commandHints,false,extra)","app.OptValue()","literal"}){
    const auto r=runtime.Evaluate(expression);Check(!r.passed && !r.error.empty() && Read(options)==saved && !runtime.Status().commandHints,"invalid original function changed real settings or appeared successful");
   }
   // Ordinary file-I/O failure in an isolated, clearly synthetic fixture.
   Check(rename(options.c_str(),(options+".saved").c_str())==0 && mkdir(options.c_str(),0700)==0,"cannot create owned save-failure fixture");
   const auto failed=runtime.Evaluate("app.SetOption(ui.commandHints,true)");Check(!failed.passed && !failed.runtime.lastSaveCommitted && !failed.runtime.commandHints && Read(options+".saved")==saved,"save failure changed committed options or appeared successful");
   Check(rmdir(options.c_str())==0 && rename((options+".saved").c_str(),options.c_str())==0,"cannot restore owned save-failure fixture");
   for(unsigned i=0;i<32;++i){const auto value=std::to_string(i);Eval(runtime,"app.SetOption(synthetic.counter,"+value+")");Check(Eval(runtime,"app.OptValue(synthetic.counter)")==value && Read(options).find("synthetic.counter="+value)!=std::string::npos,"original option round trip mismatch");++combinations;}
   std::atomic<unsigned> failures{0};std::vector<std::thread> threads;
   for(unsigned i=0;i<4;++i)threads.emplace_back([&,i]{for(unsigned j=0;j<16;++j){if(i%2){auto r=runtime.Evaluate("app.OptValue(synthetic.counter)");if(!r.passed || r.text!="31")++failures;}else{auto r=ExecuteDesktopCommand("pc.mesh torus");if(!r.passed || r.state.primitive!=1)++failures;}}});
   for(auto& t:threads)t.join();Check(failures==0 && olex2::IOlex2Processor::GetInstance()==nullptr,"shared original processor guard failed across platform consumers");
   Check(TBasicApp::HasInstance(),"actual TBasicApp did not remain alive");
  }
  Check(!TBasicApp::HasInstance(),"original runtime singleton was not released");
  {ApplicationRuntime restarted(owned.root,license);auto s=restarted.Status();Check(s.initialized && s.optionsRestored && s.originalFunctionCount==22 && !s.commandHints && s.saveRevision==0 && Eval(restarted,"app.OptValue(synthetic.counter)")=="31","fresh runtime did not restore real original settings");}
  Check(!TBasicApp::HasInstance(),"restarted original runtime singleton was not released");
  for(unsigned cycle=0;cycle<8;++cycle){
   {ApplicationRuntime repeated(owned.root,license);const auto s=repeated.Status();Check(s.originalCoreInitialized && s.originalCoreEmptyFile && s.originalCoreFileCount==1 && s.originalCoreMacroCount==135 && s.originalCoreFunctionCount==38 && s.originalFunctionCount==22 && s.optionsRestored && !s.commandHints && s.saveRevision==0,"repeated genuine core startup did not preserve empty core and original settings");}
   Check(!TBasicApp::HasInstance(),"repeated original core lifecycle left the singleton alive");
  }
  const auto beforeFailure=Read(options);Write(options,"ui.commandHints=invalid\n");
  bool badSettings=false;try{ApplicationRuntime invalidSettings(owned.root,license);}catch(const std::exception&){badSettings=true;}
  Check(badSettings && !TBasicApp::HasInstance(),"invalid private settings left a partially initialized original core singleton");
  Check(Read(options)=="ui.commandHints=invalid\n","failed startup rewrote private settings");Write(options,beforeFailure);
  {ApplicationRuntime recovered(owned.root,license);Check(recovered.Status().originalCoreInitialized && recovered.Status().originalCoreEmptyFile && !recovered.Status().commandHints,"original core did not restart after the rejected private settings fixture");}
  Check(!TBasicApp::HasInstance(),"recovered original core lifecycle left the singleton alive");
  bool bad=false;try{ApplicationRuntime invalid(owned.root,"not the licence");}catch(const std::exception&){bad=true;}Check(bad && !TBasicApp::HasInstance(),"invalid packaged resource accepted or left a runtime");
  char after[4096];Check(getcwd(after,sizeof(after)) && std::string(after)==cwd && (getenv("PATH")?getenv("PATH"):"")==path,"platform runtime changed working directory or process PATH");
  std::cout<<"PASS original application runtime; "<<assertions<<" assertions; "<<combinations<<" original option round trips; 64 mixed serialized interpreter calls; real TXApp empty startup/135 core macros/38 core functions (registered only), TBasicApp/22 original app functions; 8 repeated lifecycles and failed-settings recovery; atomic private settings and exact original licence; not complete application\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}catch(const TExceptionBase& e){std::cerr<<"FAIL "<<e.GetException()->GetFullMessage().c_str()<<'\n';return 1;}
}
