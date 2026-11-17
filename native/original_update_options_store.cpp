#include "update_options_store.h"
#include "update_options_persistence.h"
#include "updateapi.h"
#include "etime.h"
#include "eutf8.h"
#include <stdexcept>
namespace olex2::harmony::ui {
namespace {
std::string Bytes(const olxstr& text){auto value=TUtf8::Encode(text);return {value.c_str(),value.Length()};}
class OriginalUpdateOptionsStore final:public UpdateOptionsStore {
  updater::UpdateAPI api_;
public:
  OriginalUpdateOptionsStore(){
    auto& settings=api_.GetSettings();
    // The upstream missing-file path leaves these two primitives uninitialized.
    if(!settings.IsValid()){settings.last_updated=0;settings.ask_for_update=true;settings.update_interval="Always";}
  }
  UpdateOptionsSnapshot Load()override{
    const auto& s=api_.GetSettings();UpdateOptionsSnapshot result;
    TStrList mirrors;api_.GetAvailableMirrors(mirrors);
    for(size_t i=0;i<mirrors.Count();++i)result.repositories.Add(api_.AddTagPart(mirrors[i],true));
    // A fresh installation has no mirrors file; the original UI would then have
    // an empty repository list and refuse to save. Populate the original default
    // repositories and preselect one so the dialog can be applied.
    if(result.repositories.IsEmpty()){
      const TStrList& defaults=updater::UpdateAPI::GetDefaultRepositories();
      for(size_t i=0;i<defaults.Count();++i)result.repositories.Add(api_.AddTagPart(defaults[i],true));
    }
    olxstr repository=api_.AddTagPart(s.repository,true);
    if(repository.IsEmpty()&&!result.repositories.IsEmpty())repository=result.repositories[0];
    result.values={s.proxy,repository,s.update_interval,s.ask_for_update};
    result.lastUpdatedCaption=olxstr("Last updated: ")+(s.last_updated?TETime::FormatDateTime(s.last_updated):olxstr("Never"));
    return result;
  }
  void Save(const UpdateOptionsValues& values)override{
    try{SaveChecked(values);}catch(const std::exception& e){TBasicApp::NewLogEntry(logError)<<"update-options save failed: "<<e.what();throw;}
  }
  void SaveChecked(const UpdateOptionsValues& values){
    auto& original=api_.GetSettings();updater::SettingsFile candidate=original;
    candidate.proxy=values.proxy;candidate.repository=api_.TrimTagPart(values.repository);candidate.update_interval=values.interval;candidate.ask_for_update=values.askForUpdate;
    // A settings file that did not exist yet leaves source_file empty; fall back
    // to the original settings path so the first save can create it.
    const olxstr destination=original.source_file.IsEmpty()?updater::UpdateAPI::GetSettingsFileName():original.source_file;
    SaveUpdateOptionsAtomically(Bytes(destination),[&](const std::string& temporary){candidate.source_file=TUtf8::Decode(temporary.c_str());return candidate.Save();});
    // Only publish the new in-memory settings after actual atomic replacement.
    original.source_file=destination;
    original.proxy=values.proxy;original.repository=api_.TrimTagPart(values.repository);original.update_interval=values.interval;original.ask_for_update=values.askForUpdate;
  }
};
}
std::shared_ptr<UpdateOptionsStore> CreateOriginalUpdateOptionsStore(){return std::make_shared<OriginalUpdateOptionsStore>();}
}
