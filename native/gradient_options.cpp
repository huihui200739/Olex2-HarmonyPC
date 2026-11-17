#include "gradient_options.h"
#include "gldefs.h"
#include <mutex>
#include <utility>
namespace olex2::harmony::ui {
uint32_t ToOriginalRgba(Color c){return OLX_RGBA(c.red,c.green,c.blue,c.alpha);}
Color FromOriginalRgba(uint32_t v){return {OLX_GetRValue(v),OLX_GetGValue(v),OLX_GetBValue(v),OLX_GetAValue(v)};}
struct GradientStoreLifetime::State final:GradientStore {
  std::mutex gate;
  std::function<GradientColors()> read;
  std::function<void(const GradientColors&)> write;
  State(std::function<GradientColors()> r,std::function<void(const GradientColors&)> w):read(std::move(r)),write(std::move(w)){}
  GradientColors Load()override{std::lock_guard<std::mutex> lock(gate);if(!read)throw std::runtime_error("Original background owner is no longer available");return read();}
  void Save(const GradientColors& colors)override{std::lock_guard<std::mutex> lock(gate);if(!write)throw std::runtime_error("Original background owner is no longer available");write(colors);}
};
GradientStoreLifetime::GradientStoreLifetime(std::function<GradientColors()> read,std::function<void(const GradientColors&)> write){
  if(!read||!write)throw std::invalid_argument("Background access requires both original read and write operations");
  state_=std::make_shared<State>(std::move(read),std::move(write));store_=state_;
}
GradientStoreLifetime::~GradientStoreLifetime(){Revoke();}
void GradientStoreLifetime::Revoke()noexcept{std::lock_guard<std::mutex> lock(state_->gate);state_->read={};state_->write={};}
}
