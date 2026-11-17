#pragma once
#include "native_widget.h"
#include "native_ui_scheduler.h"
#include <map>
namespace olex2::harmony::ui {
constexpr int DialogCancelled=4; // Native result namespace; timeout result 0 remains unspecified.
// Native geometry is in the backend's layout units (ArkUI vp). Measured sizes
// are kept separately in physical layout pixels; they are never saved as vp.
struct DialogGeometry {Point offset{0,0};Size size{480,320};bool centered=true;};
class DialogPositionStore {
  std::thread::id thread_=std::this_thread::get_id();
  std::map<std::string,DialogGeometry> positions_;
  void Check()const;
public:
  bool Restore(const std::string&,DialogGeometry&)const;
  void Save(const std::string&,const DialogGeometry&);
};
struct DialogView {
  std::shared_ptr<Widget> root;
  std::vector<std::shared_ptr<Widget>> descendants;
  ~DialogView();
  void Retain(const std::shared_ptr<Widget>&);
};
struct DialogOptions {DialogGeometry geometry;bool modal=false;};
struct DialogCallbacks {
  std::function<void(int)> dismiss;
  std::function<void()> closed;
  std::function<void(const std::string&)> error;
};
class DialogSession {
public:
  virtual ~DialogSession()=default;
  virtual void Show()=0;
  // Requests a close. Successful return is not an unmount/completion event.
  // Keep ownership until closed() observes actual disappearance and detachment.
  virtual void Close()=0;
  virtual bool IsClosed()const=0;
  virtual bool IsClosePending()const=0;
  virtual std::string LastError()const=0;
};
class DialogBackend {
public:
  virtual ~DialogBackend()=default;
  virtual std::shared_ptr<DialogSession> Create(std::shared_ptr<DialogView>,
    const DialogOptions&,DialogCallbacks)=0;
};
std::shared_ptr<DialogBackend> CreateArkUiDialogBackend(std::shared_ptr<UiScheduler>);
class DialogWindow {
  struct State;
  std::shared_ptr<State> state_;
protected:
  void SetResizeReceiver(std::function<void()>);
public:
  DialogWindow(NodeBackend&,std::shared_ptr<DialogBackend>,std::shared_ptr<UiScheduler>,
    Widget* parent,const std::string& title,const std::string& className,
    DialogGeometry geometry={},std::shared_ptr<DialogPositionStore> positions={},bool resizable=true);
  virtual ~DialogWindow();
  DialogWindow(const DialogWindow&)=delete;
  DialogWindow& operator=(const DialogWindow&)=delete;
  Widget& Window()const;
  Widget& Body()const;
  Widget* GetParent()const;
  NodeHandle Handle()const{return Window().Handle();}
  std::shared_ptr<DialogView> SharedView()const;
  void Retain(const std::shared_ptr<Widget>&);
  bool Show(bool show=true);
  // A true native modal is asynchronous. Completion carries the actual result
  // only after unmount/detach; no nested loop or invented synchronous result.
  void ShowModalEx(bool manageParent,std::function<void(int)> completion);
  void EndModal(int result);
  void RetryClose();
  bool IsShown()const;
  bool IsClosing()const;
  bool IsModal()const;
  DialogGeometry Geometry()const;
  void SetPosition(Point offset,bool centered=true);
  void SetSize(Size);
  int GetWidth()const{return Geometry().size.width;}
  int GetHeight()const{return Geometry().size.height;}
  int GetLeft()const{return Geometry().offset.x;}
  int GetTop()const{return Geometry().offset.y;}
  void SetTitle(const std::string&);
  std::string GetTitle()const;
  std::string LastDialogError()const;
};
}
