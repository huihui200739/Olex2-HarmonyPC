#pragma once
#include "native_widget.h"
#include <array>
#include <functional>
#include <memory>
#include <stdexcept>
namespace olex2::harmony::ui {
// Original dialog order: A=LT (bottom left), B=RT (bottom right),
// C=RB (top right), D=LB (top left). These names are not screen coordinates.
using GradientColors=std::array<Color,4>;
uint32_t ToOriginalRgba(Color);
Color FromOriginalRgba(uint32_t);
class GradientSaveError:public std::runtime_error {
  bool uncertain_;
public:
  GradientSaveError(const char* message,bool uncertain):std::runtime_error(message),uncertain_(uncertain){}
  bool BackgroundMayHaveChanged()const noexcept{return uncertain_;}
};
class GradientStore {
public:
  virtual ~GradientStore()=default;
  virtual GradientColors Load()=0;
  virtual void Save(const GradientColors&)=0;
};
// The runtime owns this revocation guard, not the popup. Each operation is
// serialized, and destruction waits for in-flight access before clearing the
// callbacks. Stores can outlive the runtime but then fail without dereferencing it.
class GradientStoreLifetime {
  struct State;
  std::shared_ptr<State> state_;
  std::shared_ptr<GradientStore> store_;
public:
  GradientStoreLifetime(std::function<GradientColors()>,std::function<void(const GradientColors&)>);
  ~GradientStoreLifetime();
  GradientStoreLifetime(const GradientStoreLifetime&)=delete;
  GradientStoreLifetime& operator=(const GradientStoreLifetime&)=delete;
  std::shared_ptr<GradientStore> Store()const{return store_;}
  // Do not revoke from inside either callback; the runtime revokes before its
  // renderer/mutex are destroyed, without holding the renderer mutex.
  void Revoke()noexcept;
};
// Keep the exact original value type for rollback (including non-byte floats).
// A setter may mutate before throwing, so restore the failing corner as well.
// Rollback failures remain explicit, rather than claiming an atomic commit.
template<class Value,class Read,class Write>
void ApplyGradientWithRollback(const std::array<Value,4>& next,Read read,Write write){
  const std::array<Value,4> before{{read(0),read(1),read(2),read(3)}};
  size_t touched=0;
  try{for(size_t i=0;i<4;++i){touched=i+1;write(i,next[i]);}}
  catch(...){
    bool restored=true;
    for(size_t i=0;i<touched;++i){try{write(i,before[i]);}catch(...){restored=false;}}
    throw GradientSaveError(restored?"Background update failed; previous colors restored":"Background update and rollback failed; some colors may have changed",!restored);
  }
}
}
