#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace olex2::harmony::ui {
struct MenuRow {
  int id=0;
  std::string label,help;
  bool separator=false,enabled=true,checkable=false,checked=false,submenu=false;
};
// An owned, real popup in production. Host tests inject an explicitly separate
// presenter; there is no recording implementation in the shipping target.
class MenuPopup {
public:
  virtual ~MenuPopup()=default;
  virtual void Open(const std::string& title,const std::vector<MenuRow>&,
                    std::function<void(int)> chosen)=0;
  virtual void Close()=0;
};
std::shared_ptr<MenuPopup> CreateArkUiMenuPopup();
}
