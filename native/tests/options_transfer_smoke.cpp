// Ordinary options-file interoperability in an exclusively owned synthetic tree.
#include "application_runtime.h"
#include "integration.h"
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
using namespace olex2::harmony;
namespace {
unsigned assertions=0;
void Check(bool passed,const char* message){if(!passed)throw std::runtime_error(message);++assertions;}
std::string Read(const std::string& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Cannot read owned fixture");return {std::istreambuf_iterator<char>(f),{}};}
void Write(const std::string& path,const std::string& value){std::ofstream f(path,std::ios::binary);f<<value;if(!f)throw std::runtime_error("Cannot write owned fixture");}
struct Owned {
 std::string root;
 explicit Owned(const std::string& parent){std::string s=parent+"/synthetic-options-transfer-XXXXXX";std::vector<char>b(s.begin(),s.end());b.push_back(0);char* p=mkdtemp(b.data());if(!p)throw std::runtime_error("Cannot create owned synthetic fixture");root=p;}
 ~Owned(){const auto runtime=root+"/olex2-runtime-v1";for(const char* file:{"config/.options","config/.options.saved","base/OLEX2-LICENCE.txt"})unlink((runtime+"/"+file).c_str());for(const char* dir:{"config","instance","shared","base"})rmdir((runtime+"/"+dir).c_str());rmdir(runtime.c_str());rmdir(root.c_str());}
};
}
int main(int argc,char** argv){
 try{
  if(argc!=3)throw std::invalid_argument("Expected owned test parent and original licence");
  const std::string licence=Read(argv[2]);Owned owned(argv[1]);
  const auto config=owned.root+"/olex2-runtime-v1/config",path=config+"/.options";
  std::string savedHash;
  {
   ApplicationRuntime app(owned.root,licence);auto before=app.Status();
   const auto fresh=app.ExportOptions();
   Check(fresh.passed && fresh.bytes.find("ui.commandHints=true")!=std::string::npos && fresh.sha256.size()==64,"fresh export did not include effective platform default");
   Check(fresh.runtime.optionsCount==0 && fresh.runtime.saveRevision==0 && fresh.runtime.optionsSha256.empty() && access(path.c_str(),F_OK)!=0,"read-only export committed private options");
   const auto imported=app.ImportOptions("# isolated synthetic import\nui.commandHints=false\nsynthetic.label=\xE4\xB8\xAD\xE6\x96\x87\n");
   if(!imported.passed)std::cerr<<"Initial import error: "<<imported.error<<"\n";
   Check(imported.passed && imported.optionsChanged && imported.importedOptions==2 && !imported.runtime.commandHints && imported.runtime.lastSaveCommitted && imported.runtime.saveRevision==1,"valid original-format document was not merged and committed");
   Check(Read(path).find("synthetic.label=")!=std::string::npos && app.Evaluate("app.OptValue(synthetic.label)").text=="\xE4\xB8\xAD\xE6\x96\x87","Unicode setting did not survive original parser/serializer");
   // Existing original-format comments and unknown keys must remain on merge.
   const std::string decorated="# original retained comment\n"+Read(path)+"unknown.keep=value\n";Write(path,decorated);
   const auto merge=app.ImportOptions("ui.commandHints=true\nsynthetic.counter=1\n");
   Check(!merge.passed && !merge.optionsChanged && !merge.runtime.commandHints && merge.runtime.optionsCount==2,"live external file/map disagreement was not rejected without changing memory");
   Check(Read(path)==decorated,"rejected live external-file disagreement changed existing comments or keys");
   // Reloaded external unknown keys are intentionally not silently discarded.
   // The fixture above is an ordinary external edit while the app is running:
   // the save hook must reject a map/file disagreement rather than misreport it.
  }
  {ApplicationRuntime app(owned.root,licence);savedHash=app.Status().optionsSha256;
   const auto first=app.ImportOptions("ui.commandHints=false\nsynthetic.duplicate=first\nsynthetic.duplicate=last\n");
   Check(first.passed && app.Evaluate("app.OptValue(synthetic.duplicate)").text=="last","original most-recent duplicate rule not used");
   const auto snapshot=app.ExportOptions();const auto state=app.Status();const auto saved=Read(path);
   Check(snapshot.passed && snapshot.bytes.find("# original retained comment")!=std::string::npos && snapshot.bytes.find("unknown.keep=value")!=std::string::npos,"export lost retained original comments/keys");
   Check(app.Status().saveRevision==state.saveRevision && Read(path)==saved,"snapshot export changed committed settings");
   for(const auto& invalid:std::vector<std::string>{"", "# no keys\n", "ui.commandHints=invalid\n", "bad key=value\n", "ui.commandHints=true\nvalid.one=ok\nbad key=value\n",std::string("x=y\0z=w",7),std::string(MaximumOptionsTransferBytes+1,'a')}){
    const auto fail=app.ImportOptions(invalid);Check(!fail.passed && !fail.optionsChanged && !fail.error.empty() && app.Status().optionsSha256==state.optionsSha256 && Read(path)==saved && !app.Status().commandHints,"invalid ordinary settings document partially changed committed options");
   }
   // 257 ordinary names exceed the documented merge count, without side effects.
   std::string tooMany;for(unsigned i=0;i<257;++i)tooMany+="synthetic.key"+std::to_string(i)+"=value\n";
   const auto count=app.ImportOptions(tooMany);Check(!count.passed && !count.optionsChanged && Read(path)==saved,"over-budget merge partially applied");
   Check(rename(path.c_str(),(path+".saved").c_str())==0 && mkdir(path.c_str(),0700)==0,"cannot create ordinary owned file-I/O failure fixture");
   const auto failed=app.ImportOptions("ui.commandHints=true\n");Check(!failed.passed && !failed.optionsChanged && !failed.runtime.commandHints && Read(path+".saved")==saved,"failed private save committed imported options");
   Check(rmdir(path.c_str())==0 && rename((path+".saved").c_str(),path.c_str())==0,"cannot restore own fixture");
   DIR* dir=opendir(config.c_str());Check(dir!=nullptr,"cannot inspect own staging directory");bool leftover=false;while(auto* e=readdir(dir)){std::string name=e->d_name;if(name.find(".tmp-")!=std::string::npos)leftover=true;}closedir(dir);Check(!leftover,"transfer left owned staging files");
  }
  {ApplicationRuntime app(owned.root,licence);Check(app.Status().optionsRestored && !app.Status().commandHints && app.Evaluate("app.OptValue(synthetic.duplicate)").text=="last","restart did not restore imported original-format options");}
  std::cout<<"PASS original options document transfer; "<<assertions<<" assertions; read-only original-format export, validated key merge, exact Unicode, failure preservation and restart; not full application documents or system-picker verification\n";
 }catch(const TExceptionBase& e){std::cerr<<"FAIL "<<e.GetException()->GetFullMessage().c_str()<<'\n';return 1;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
