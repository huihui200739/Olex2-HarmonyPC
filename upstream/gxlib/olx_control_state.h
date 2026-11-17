/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/
#pragma once
#include "gxbase.h"
#include "estlist.h"
#undef Status
BeginGxlNamespace()
class TStateChange: public IOlxObject {
  const size_t State;
  const bool Status;
  const olxstr Data;
public:
  TStateChange(size_t state, bool status, const olxstr& data=EmptyString())
  : State(state), Status(status), Data(data)
  {}
  bool GetStatus() const {  return Status;  }
  size_t GetState() const {  return State;  }
  const olxstr& GetData() const {  return Data;  }
};

class TModeChange: public IOlxObject  {
  bool FStatus;
  size_t Mode;
public:
  TModeChange(size_t mode, bool status)
    : FStatus(status), Mode(mode)
  {}
  ~TModeChange()  {}
  bool GetStatus() const {  return FStatus;  }
};

EndGxlNamespace()
