#pragma once
#include "native_picker.h"
#include <arkui/native_dialog.h>
#include <arkui/native_node.h>
#include <array>
#include <memory>
#include <utility>
namespace olex2::harmony::ui {
// Genuine API12 NativeDialog graphs. The calendar uses native buttons and a
// native year editor for the complete 1..9999 civil range; RGBA uses four native
// sliders, a hex editor and explicit confirmation/cancellation.
class ArkUiPickerPopup {
public:
  enum class Kind {Date,Color};
private:
  enum class Action {Day,PreviousMonth,NextMonth,PreviousYear,NextYear,Year,Today,None,Cancel,ColorChannel,Hex,ConfirmColor};
  struct Binding {ArkUiPickerPopup* owner=nullptr;ArkUI_NodeHandle node=nullptr;Action action=Action::Cancel;int index=0;std::vector<ArkUI_NodeEventType> events;bool receiverAttached=false;};
  ArkUI_NativeNodeAPI_1& api_;
  ArkUI_NodeHandle root_;
  Kind kind_;
  std::function<void(const NodeEvent&)> receiver_;
  ArkUI_NativeDialogAPI_1* dialogs_=nullptr;
  ArkUI_NativeDialogHandle dialog_=nullptr;
  bool shown_=false,contentAttached_=false,closing_=false,updating_=false,readOnly_=false,allowEmpty_=false;
  CalendarDate date_,view_;
  Color color_,candidate_;
  std::vector<ArkUI_NodeHandle> nodes_;
  std::vector<std::pair<ArkUI_NodeHandle,ArkUI_NodeHandle>> edges_;
  std::vector<std::unique_ptr<Binding>> bindings_;
  ArkUI_NodeHandle panel_=nullptr,year_=nullptr,month_=nullptr,hex_=nullptr,preview_=nullptr;
  std::array<ArkUI_NodeHandle,42> days_{};
  std::array<int,42> dayNumbers_{};
  std::array<ArkUI_NodeHandle,4> sliders_{},channelValues_{};
  std::string yearDraft_,hexDraft_;
  ArkUI_NodeHandle Node(ArkUI_NodeType,float,float);
  void Child(ArkUI_NodeHandle,ArkUI_NodeHandle);
  void Bind(ArkUI_NodeHandle,Action,int,std::initializer_list<ArkUI_NodeEventType>);
  ArkUI_NodeHandle Button(ArkUI_NodeHandle,const std::string&,float,Action,int=0);
  ArkUI_NodeHandle Label(ArkUI_NodeHandle,const std::string&,float,float=28);
  void BuildCalendar();void BuildColor();
  void RefreshCalendar();void RefreshColor();
  void CommitYear();void CommitHex();
  void SetLabel();
  void AcceptDate(CalendarDate);void AcceptColor();
  void Handle(Action,int,ArkUI_NodeEvent*);
  static void Receive(ArkUI_NodeEvent*)noexcept;
public:
  ArkUiPickerPopup(ArkUI_NativeNodeAPI_1&,ArkUI_NodeHandle,Kind,std::function<void(const NodeEvent&)>);
  ~ArkUiPickerPopup();
  void SetValue(const std::string&);
  std::string Value()const;
  void SetReadOnly(bool);
  void SetAllowEmpty(bool);
  void Open();void Close();
};
}
