#include "native_widget.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace olex2::harmony::ui {
void Widget::AssertThread()const{if(std::this_thread::get_id()!=thread_)throw std::runtime_error("Native controls require their creating UI thread");}
Widget::Widget(NodeBackend& backend,NodeKind kind,Widget* parent,int id,Point pos,Size size,long style)
  :backend_(backend),kind_(kind),thread_(std::this_thread::get_id()) {
  if(parent && &parent->Backend()!=&backend)throw std::invalid_argument("Control and parent backends differ");
  node_=backend_.Create(kind);if(!node_)throw std::runtime_error("Native control creation failed");
  try {
    SetNumber(Property::Enabled,1);SetNumber(Property::Visible,1);
    if(id>=0)SetText(TextProperty::Id,"olex2-original-control-"+std::to_string(id));
    if(pos.x>=0)SetLeft(pos.x);if(pos.y>=0)SetTop(pos.y);
    if(size.width>=0)SetWidth(size.width);if(size.height>=0)SetHeight(size.height);
    if(kind==NodeKind::Slider)SetWindowStyle(style);else if(style)throw std::invalid_argument("Unsupported native control style");
    if(parent){parent->AssertThread();parent->children_.reserve(parent->children_.size()+1);backend_.AddChild(parent->node_,node_);parent->children_.push_back(this);parent_=parent;if(parent->hasLifetime_){lifetime_=parent->lifetime_;hasLifetime_=true;}}
  }catch(...){backend_.Destroy(node_);node_=nullptr;throw;}
}
Widget::~Widget(){
  objectLife_.reset();if(!node_)return;
  backend_.Unsubscribe(node_);
  for(auto* child:children_){try{backend_.RemoveChild(node_,child->node_);}catch(...){}child->parent_=nullptr;}
  if(parent_){try{backend_.RemoveChild(parent_->node_,node_);}catch(...){}auto& c=parent_->children_;c.erase(std::remove(c.begin(),c.end(),this),c.end());}
  backend_.Destroy(node_);
}
void Widget::Listen(std::vector<EventKind> events){
  AssertThread();std::weak_ptr<void> object=objectLife_;backend_.Subscribe(node_,events,[this,object](const NodeEvent& event){
    if(object.expired())return;std::shared_ptr<void> guard;if(hasLifetime_){guard=lifetime_.lock();if(!guard)return;}
    try{AssertThread();if(event.kind==EventKind::BackendError)throw std::runtime_error(event.text);if(IsEventAllowed())OnNativeEvent(event);else OnInteractionBlocked(event);}
    catch(const std::exception& error){if(!object.expired())lastError_=error.what();}
    catch(...){if(!object.expired())lastError_="Original control callback failed";}
  });
}
void Widget::SetEventLifetime(const std::shared_ptr<void>& value){AssertThread();lifetime_=value;hasLifetime_=true;for(auto* child:children_)child->SetEventLifetime(value);}
bool Widget::IsEventAllowed()const{AssertThread();for(auto* node=this;node;node=node->parent_)if(!node->IsEnabled()||!node->IsShown())return false;return true;}
void Widget::SetNumber(Property property,double value){AssertThread();if(!std::isfinite(value))throw std::invalid_argument("Control value is not finite");backend_.SetNumber(node_,property,value);}
double Widget::GetNumber(Property property)const{AssertThread();return backend_.GetNumber(node_,property);}
void Widget::SetText(TextProperty property,const std::string& value){AssertThread();backend_.SetText(node_,property,value);}
std::string Widget::GetText(TextProperty property)const{AssertThread();return backend_.GetText(node_,property);}
void Widget::SetWindowStyle(long value){AssertThread();if(kind_!=NodeKind::Slider){if(value)throw std::invalid_argument("Unsupported native control style");style_=0;return;}if(value&~Vertical)throw std::invalid_argument("Unsupported native slider style");SetNumber(Property::SliderVertical,(value&Vertical)?1:0);style_=value;}
void Widget::SetExtraStyle(long value){AssertThread();if(value)throw std::invalid_argument("No native extra style was requested by this port");}
}
