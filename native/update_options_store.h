#pragma once
#include "estrlist.h"
#include <memory>
namespace olex2::harmony::ui {
struct UpdateOptionsValues {
  olxstr proxy,repository,interval;
  bool askForUpdate=true;
};
struct UpdateOptionsSnapshot {
  UpdateOptionsValues values;
  TStrList repositories;
  olxstr lastUpdatedCaption;
};
class UpdateOptionsStore {
public:
  virtual ~UpdateOptionsStore()=default;
  virtual UpdateOptionsSnapshot Load()=0;
  virtual void Save(const UpdateOptionsValues&)=0;
};
// Production adapter retains the original UpdateAPI/SettingsFile implementation.
// It reads/writes settings only; it does not download, install or run commands.
std::shared_ptr<UpdateOptionsStore> CreateOriginalUpdateOptionsStore();
}
