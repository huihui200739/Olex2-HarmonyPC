#pragma once
// Platform-neutral ownership seam, used by real ArkUI nodes in the shipped
// target. A separate recording backend is used only by ordinary host unit tests.
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <stdexcept>
#include <thread>
#include <vector>
#include <utility>
namespace olex2::harmony::ui {
using NodeHandle=void*;
enum class NodeKind { Row, Text, Slider, CheckBox, Choice, Column, Button, TextInput, TextArea, ListBox, DateField, ColorField, Image, Tree, Scroll };
enum class Property { Width,Height,Left,Top,Background,Enabled,Visible,Focusable,
  FontColor,FontSize,SliderValue,SliderMinimum,SliderMaximum,SliderStep,
  SliderVertical,SliderTrackColor,SliderSelectedColor,RowSpacing,Checked,ChoiceSelection,BorderWidth,BorderColor,BorderRadius,Editable,TextAlign,CompactOptions,PickerReadOnly,PickerAllowEmpty,Focused,Padding,ButtonRectangular,ImageStretch,TreeScrollX,TreeScrollY,LayoutWeight,TranslateX,TranslateY };
enum class TextProperty { Content,Accessibility,Id };
enum class EventKind { Click,SliderChange,CheckChange,ChoiceChange,Focus,Blur,Hover,TextChange,Submit,Key,DoubleClick,SpinStep,BackendError,DateChange,ColorChange,Pointer,TreeSelect,TreeToggle,TreeActivate,TreeContext,TreeEdit };
enum class SliderPhase { Begin=0,Move=1,End=2,Click=3 };
enum class LogicalKey { Other,Up,Down,Left,Right,Home,End,Enter,Space,Escape,Tab,PageUp,PageDown,Backspace,Delete };
struct KeyEvent {
  int code=0; LogicalKey logical=LogicalKey::Other; uint32_t unicode=0;
  bool down=false,shift=false,control=false,alt=false,meta=false,handled=false;
  std::string text;
  int GetKeyCode()const{return code;} uint32_t GetUnicodeKey()const{return unicode;}
  bool ShiftDown()const{return shift;} bool ControlDown()const{return control;}
  bool AltDown()const{return alt;} bool MetaDown()const{return meta;}
  void Skip(bool value=true){handled=!value;}
};
enum class PointerPhase { Down,Move,Up,Cancel };
struct PointerEvent {PointerPhase phase=PointerPhase::Cancel;int id=-1;double x=0,y=0;bool inside=false;double windowX=0,windowY=0;};
struct NodeEvent { EventKind kind; double value=0;SliderPhase phase=SliderPhase::Move; std::string text; KeyEvent* key=nullptr; PointerEvent* pointer=nullptr; };
struct Point {int x=-1,y=-1;};
struct Size {int width=-1,height=-1;};
struct Color {uint8_t red=0,green=0,blue=0,alpha=255;};

class ImageAsset {
public:
 const uint32_t width,height;
 ImageAsset(uint32_t w,uint32_t h):width(w),height(h){if(!w||!h)throw std::invalid_argument("Image dimensions must be positive");}
 virtual ~ImageAsset()=default;
};
using ImageRef=std::shared_ptr<const ImageAsset>;
struct TextSelection {int32_t start=0,end=0;}; // Native UTF-16 code units.
struct TreeRow {std::string label;size_t depth=0;bool expandable=false,expanded=false,selected=false,editing=false;};
constexpr long Vertical=1;
class NodeBackend {
public:
  virtual ~NodeBackend()=default;
  virtual NodeHandle Create(NodeKind)=0;
  virtual void Destroy(NodeHandle) noexcept=0;
  virtual void AddChild(NodeHandle,NodeHandle)=0;
  virtual void RemoveChild(NodeHandle,NodeHandle)=0;
  virtual void SetNumber(NodeHandle,Property,double)=0;
  virtual double GetNumber(NodeHandle,Property)const=0;
  virtual void SetText(NodeHandle,TextProperty,const std::string&)=0;
  virtual std::string GetText(NodeHandle,TextProperty)const=0;
  virtual void Subscribe(NodeHandle,const std::vector<EventKind>&,std::function<void(const NodeEvent&)>)=0;
  virtual void Unsubscribe(NodeHandle)noexcept=0;
  virtual Size Measure(NodeHandle)const{throw std::runtime_error("Backend does not implement measured native geometry");}
  virtual void SetTextSelection(NodeHandle,TextSelection){throw std::runtime_error("Backend does not implement native text selection");}
  virtual int32_t GetTextCaret(NodeHandle)const{throw std::runtime_error("Backend does not implement a native active text caret");}
  virtual TextSelection GetTextSelection(NodeHandle)const{throw std::runtime_error("Backend does not implement native text selection");}
  virtual void SetOptions(NodeHandle,const std::vector<std::string>&){throw std::runtime_error("Backend does not implement native option ranges");}
  virtual void SetTreeRows(NodeHandle,const std::vector<TreeRow>&){throw std::runtime_error("Backend does not implement native hierarchical tree rows");}
  virtual ImageRef DecodeImage(const std::vector<uint8_t>&){throw std::runtime_error("Backend does not implement native image decoding");}
  virtual void SetImage(NodeHandle,const ImageRef&){throw std::runtime_error("Backend does not implement a native image node");}
  virtual void OpenOptions(NodeHandle){throw std::runtime_error("Backend does not implement a native option popup");}
  virtual void CloseOptions(NodeHandle){throw std::runtime_error("Backend does not implement a native option popup");}
  virtual void OpenPicker(NodeHandle){throw std::runtime_error("Backend does not implement a native calendar/color popup");}
  virtual void ClosePicker(NodeHandle){throw std::runtime_error("Backend does not implement a native calendar/color popup");}
};
class Widget {
  NodeBackend& backend_;
  NodeKind kind_;
  NodeHandle node_=nullptr;
  Widget* parent_=nullptr;
  std::vector<Widget*> children_;
  std::shared_ptr<void> objectLife_=std::make_shared<int>(0);
  std::weak_ptr<void> lifetime_;
  bool hasLifetime_=false;
  long style_=0;
  std::thread::id thread_;
  std::string lastError_;
protected:
  void Listen(std::vector<EventKind> events);
  virtual void OnNativeEvent(const NodeEvent&) {}
  virtual void OnInteractionBlocked(const NodeEvent&) {}
  void AssertThread()const;
  void RememberWindowStyle(long value){AssertThread();style_=value;}
public:
  Widget(NodeBackend&,NodeKind,Widget* parent=nullptr,int id=-1,Point={},Size={},long style=0);
  virtual ~Widget();
  Widget(const Widget&)=delete;
  Widget& operator=(const Widget&)=delete;
  std::weak_ptr<void> ObjectLifetime()const{return objectLife_;}
  NodeHandle Handle()const{return node_;}
  NodeBackend& Backend()const{return backend_;}
  void SetEventLifetime(const std::shared_ptr<void>& value);
  bool IsEventAllowed()const;
  const std::string& LastEventError()const{return lastError_;}
  void SetNumber(Property,double);
  double GetNumber(Property)const;
  void SetText(TextProperty,const std::string&);
  std::string GetText(TextProperty)const;
  void SetImage(const ImageRef& image){AssertThread();backend_.SetImage(node_,image);}
  bool IsEnabled()const{return GetNumber(Property::Enabled)!=0;}
  void Enable(bool value){SetNumber(Property::Enabled,value?1:0);}
  bool IsShown()const{return GetNumber(Property::Visible)!=0;}
  void Show(bool value=true){SetNumber(Property::Visible,value?1:0);}
  void Hide(){Show(false);}
  int GetWidth()const{return static_cast<int>(GetNumber(Property::Width));}
  int GetHeight()const{return static_cast<int>(GetNumber(Property::Height));}
  int GetLeft()const{return static_cast<int>(GetNumber(Property::Left));}
  int GetTop()const{return static_cast<int>(GetNumber(Property::Top));}
  void SetWidth(int value){SetNumber(Property::Width,value);}
  void SetHeight(int value){SetNumber(Property::Height,value);}
  void SetLeft(int value){SetNumber(Property::Left,value);}
  void SetTop(int value){SetNumber(Property::Top,value);}
  uint32_t GetBackgroundColor()const{return static_cast<uint32_t>(GetNumber(Property::Background));}
  void SetBackgroundColor(uint32_t value){SetNumber(Property::Background,value);}
  long GetWindowStyle()const{return style_;}
  virtual void SetWindowStyle(long);
  long GetExtraStyle()const{return 0;}
  void SetExtraStyle(long);
};
class EventWidget:public Widget {
  std::function<void(const NodeEvent&)> receiver_;
protected:
  void OnNativeEvent(const NodeEvent& event)override{receiver_(event);}
public:
  EventWidget(NodeBackend& backend,NodeKind kind,Widget* parent,std::vector<EventKind> events,std::function<void(const NodeEvent&)> receiver)
    :Widget(backend,kind,parent),receiver_(std::move(receiver)){Listen(std::move(events));}
};
NodeBackend& ArkUiBackend(); // Implemented by the genuine SDK-backed target only.
}
