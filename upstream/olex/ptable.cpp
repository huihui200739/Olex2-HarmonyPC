/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(OLEX2_NATIVE_WIDGETS) && !defined(__WXWIDGETS__)
#include "ptable.h"
#include <algorithm>
#include <string>
#include "eutf8.h"

using namespace olex2::harmony::ui;

struct TPTableDlg::Alive {
  TPTableDlg* owner = nullptr;
};

TPTableDlg::TPTableDlg(NodeBackend& nodes,
  std::shared_ptr<DialogBackend> dialogs,
  std::shared_ptr<UiScheduler> scheduler, Widget* parent,
  std::shared_ptr<DialogPositionStore> positions)
  : TDialog(nodes, std::move(dialogs), std::move(scheduler), parent,
      "Periodic table", "TPTableDlg",
      DialogGeometry{Point{0, 0}, Size{740, 430}, true},
      std::move(positions)),
    alive_(std::make_shared<Alive>())
{
  alive_->owner = this;

  scroll_ = std::make_shared<Widget>(nodes, NodeKind::Scroll, &Body());
  Retain(scroll_);
  table_ = std::make_shared<Widget>(nodes, NodeKind::Column, scroll_.get());
  Retain(table_);
  table_->SetNumber(Property::RowSpacing, 0);

  for (size_t row = 0; row < rows_.size(); ++row) {
    if (row == 7) {
      spacer_ = std::make_shared<Widget>(nodes, NodeKind::Column, table_.get());
      Retain(spacer_);
      spacer_->SetWidth(1);
      spacer_->SetHeight(5);
    }

    rows_[row] = std::make_shared<Widget>(nodes, NodeKind::Row, table_.get());
    Retain(rows_[row]);
    rows_[row]->SetNumber(Property::RowSpacing, 0);
    rows_[row]->SetWidth(18 * 37);
    rows_[row]->SetHeight(37);
    for (size_t column = 0; column < cells_[row].size(); ++column) {
      cells_[row][column] = std::make_shared<Widget>(
        nodes, NodeKind::Row, rows_[row].get());
      Retain(cells_[row][column]);
      cells_[row][column]->SetWidth(37);
      cells_[row][column]->SetHeight(37);
    }
  }

  for (int row = 0; row < 9; ++row) {
    for (int column = 0; column < 18; ++column) {
      if (row == 0) {
        if (column == 0 || column == 17) CreateButton(row, column);
      }
      else if (row < 3) {
        if (column <= 1 || column >= 12) CreateButton(row, column);
      }
      else if (row < 6) {
        CreateButton(row, column);
      }
      else if (row == 6) {
        if (column <= 3) CreateButton(row, column);
      }
      else if (column <= 14) {
        CreateButton(row, column);
      }
    }
  }
  CreateButton(8, 17);  // Q peak

  for (size_t index = iQPeakIndex + 1; index <= iMaxElementIndex; ++index) {
    CreateButton(9, static_cast<int>(index - iQPeakIndex - 1));  // Deuterium
  }

  LayoutViewport();
  std::weak_ptr<Alive> weak = alive_;
  SetResizeReceiver([weak]() {
    if (auto life = weak.lock(); life && life->owner) {
      life->owner->OnResize.Execute(life->owner, nullptr);
      life->owner->LayoutViewport();
    }
  });
}

TPTableDlg::~TPTableDlg() {
  alive_->owner = nullptr;
}

void TPTableDlg::LayoutViewport() {
  const int bodyWidth = Body().GetWidth();
  const int bodyHeight = Body().GetHeight();
  scroll_->SetWidth(std::max(120, bodyWidth - 24));
  scroll_->SetHeight(std::max(40, bodyHeight - 24));
  table_->SetWidth(18 * 37);
  table_->SetHeight(10 * 37 + 5);
}

void TPTableDlg::CreateButton(int row, int column) {
  int index = static_cast<int>(++createdButtons_);
  if (index == 57 || index == 75) return;
  if (index > 57 && index < 75) index += 14;
  else if (index > 75 && index <= 76) index += 28;
  else if (index > 76 && index <= 91) index -= 20;
  else if (index > 91 && index < 107) index -= 3;
  if (index >= 107) index -= 2;

  const short elementIndex = static_cast<short>(index - 1);
  const auto& element = XElementLib::GetByIndex(elementIndex);
  const auto encoded = TUtf8::Encode(element.symbol);
  const std::string label(encoded.c_str(), encoded.Length());
  std::weak_ptr<Alive> weak = alive_;
  auto button = std::make_shared<EventWidget>(Window().Backend(),
    NodeKind::Button,
    cells_.at(static_cast<size_t>(row)).at(static_cast<size_t>(column)).get(),
    std::vector<EventKind>{EventKind::Click},
    [weak, elementIndex](const NodeEvent&) {
      if (auto life = weak.lock(); life && life->owner) {
        life->owner->Select(elementIndex);
      }
    });
  Retain(button);
  button->SetText(TextProperty::Content, label);
  button->SetText(TextProperty::Accessibility, "Element " + label);
  button->SetWidth(36);
  button->SetHeight(36);
  button->SetNumber(Property::Focusable, 1);
  button->SetNumber(Property::ButtonRectangular, 1);
  buttons_.push_back(std::move(button));
}

void TPTableDlg::Select(short index) {
  Selected = &XElementLib::GetByIndex(index);
  EndModal(1);
}
#else
#include "ptable.h"

TPTableDlg::TPTableDlg(TMainFrame *Parent) :
  TDialog(Parent, wxT("Periodic table"), EsdlClassName(TPTableDlg).u_str() )
{
  AActionHandler::SetToDelete(false);
  for( int i=0; i < 9; i++ )  {
    for( int j=0; j < 18; j++ )  {
      if( i == 0 )  {
        if( j == 0 )
          CreateButton(i,j,0);
        if( j == 17 )
          CreateButton(i,j,0);
      }
      else if( i < 3 )  {
        if( j <= 1 || j >= 12 )
          CreateButton(i,j,0);
      }
      else if( i < 6 )  {
        CreateButton(i,j,0);
      }
      else if( i == 6 )  {
        if( j <= 3  )
          CreateButton(i,j,0);
      }
      else if( i >= 6 )  {  // lantinides and actinides
        if( j <= 14  )
          CreateButton(i,j,5);
      }
    }
  }
  CreateButton(8, 17, 5 );  // q peak

  for( size_t i=iQPeakIndex+1; i <= iMaxElementIndex; i++ )
    CreateButton(9, i-iQPeakIndex-1, 5);  // q peak
  wxDialog::Fit();
  wxDialog::Center();
}
TPTableDlg::~TPTableDlg()  {
  for( size_t i =0; i < ButtonsList.Count(); i++ )  {
    ButtonsList[i]->OnClick.Clear();
    ButtonsList[i];
  }
}
//..............................................................................
void TPTableDlg::CreateButton(int i, int j, int offset )  {
  TButton *B = new TButton(this); // q peak
#ifndef __WIN32__
  short bd = 36;
#else
  short bd = 22;
#endif
  B->WI.SetLeft( j*(bd+1));
  B->WI.SetTop(   i*(bd+1)+offset);
  B->WI.SetWidth( bd );
  B->WI.SetHeight( bd );
  ButtonsList.Add(B);
  int ii = ButtonsList.Count();
  B->SetTag( ii-1 );
  if( ii == 57 || ii == 75 )
    return;
  if( ii > 57 && ii < 75 )  {
    ii += 14;
      goto data;
  }
  if( ii > 75  && ii <= 76 )  {  // 76 - last element Ku
    ii += 28;
    goto data;
  }
  if( ii > 76 && ii <= 91)  // lanthanides
    ii -= 20;
  if( ii > 91 && ii < 107)  // lanthanides and q peak
    ii -= 3;

data:
  if( ii >= 107 )
    ii-=2;
  B->SetCaption(XElementLib::GetByIndex(ii-1).symbol);
//  B->Font->Color = A->Color;
  B->OnClick.Add(this);
  B->SetTag( ii );
}
//..............................................................................
bool TPTableDlg::Execute(const IOlxObject *Sender, const IOlxObject *Data,
  TActionQueue *)
{
  const TButton *S = dynamic_cast<const TButton *>(Sender);
  Selected = &XElementLib::GetByIndex(S->GetTag()-1);
  wxDialog::EndModal(wxID_OK);
  return true;
}

#endif // native periodic table / original wx
