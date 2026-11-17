/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/
#include "tasks.h"
#if defined(__OHOS__) && !defined(__WXWIDGETS__)
#include "xapp.h"
#include "bapp.h"
#include "integration.h"
#include "ins.h"
#include "efile.h"
#include "log.h"

void P4PTask::Run() {
  if (!file_id.Contains("Rigaku")) return;
  olex2::IOlex2Processor* processor = olex2::IOlex2Processor::GetInstance();
  if (processor == nullptr) return;
  olxstr exists = "spy.xplain.exists()";
  processor->processFunction(exists, __OlxSrcInfo);
  if (!exists.IsBool() || !exists.ToBool()) return;

  const olxstr option = TBasicApp::GetInstance().GetOptions().FindValue("run_rigaku_xplain");
  // Harmony task execution is non-blocking on the ArkUI thread. Never open a
  // modal wx dialog here; without an explicit stored choice the optional XPlain
  // action remains disabled.
  if (option.IsEmpty()) {
    TBasicApp::NewLogEntry() << "Rigaku XPlain was not started because no Harmony preference was set";
    return;
  }
  if (!option.ToBool()) return;

  olxstr loaded = TXApp::GetInstance().XFile().GetFileName();
  const olxstr exts[] = {"ins", "res", "cif"};
  bool cellSourceExists = false;
  for (const auto& ext : exts) {
    if (TEFile::Exists(TEFile::ChangeFileExt(loaded, ext))) {
      cellSourceExists = true;
      break;
    }
  }
  if (!cellSourceExists) {
    TIns ins;
    ins.Adopt(TXApp::GetInstance().XFile(), 0);
    ins.SaveForSolution(TEFile::ChangeFileExt(loaded, "ins"), "TREF", "Imported by Olex2");
  }
  olxstr run = "spy.xplain.run(false, true)";
  processor->processFunction(run);
  if (run.IsBool() && run.ToBool()) processor->processMacro("reap", __OlxSrcInfo);
  else TBasicApp::NewLogEntry(logError) << "Failed to run XPlain";
}

void CellChangeTask::Run() {
  TXApp& app = TXApp::GetInstance();
  TXFile& file = app.XFile();
  if (hklsrc != file.GetRM().GetHKLSource() || !file.GetRM().GetHKLF_mat().IsI()) return;

  TAsymmUnit& current = file.GetAsymmUnit();
  olxstr oldCell, newCell;
  double axisDifference = 0;
  for (size_t i = 0; i < 3; ++i) {
    oldCell << " " << TEValueD(current.GetAxes()[i], current.GetAxisEsds()[i]).ToString()
      << " " << TEValueD(current.GetAngles()[i], current.GetAngleEsds()[i]).ToString();
    newCell << " " << TEValueD(axes[i], axis_esd[i]).ToString()
      << " " << TEValueD(angles[i], angle_esd[i]).ToString();
    axisDifference += olx_abs(current.GetAxes()[i] - axes[i]);
  }
  if (axisDifference / 3 > 1) {
    TBasicApp::NewLogEntry(logError) << "There is a new cell in the HKL file but it differs significantly from the current one - please update manually";
    return;
  }

  if (!file.HasLastLoader()) return;
  const bool useNewCell = TBasicApp::GetInstance().GetOptions()
    .FindValue("use_hkl_cell", TrueString()).ToBool();
  if (!useNewCell && oldCell != newCell) {
    TBasicApp::NewLogEntry(logInfo) << "HKL cell parameters differ; keeping the saved Harmony preference";
    return;
  }
  if (!useNewCell) return;

  TAsymmUnit& loaded = file.LastLoader()->GetAsymmUnit();
  loaded.GetAxes() = current.GetAxes() = axes;
  loaded.GetAngles() = current.GetAngles() = angles;
  loaded.GetAxisEsds() = current.GetAxisEsds() = axis_esd;
  loaded.GetAngleEsds() = current.GetAngleEsds() = angle_esd;
  TBasicApp::NewLogEntry() << "Updating cell parameters with value from HKL file";
}
#else
#include "xglapp.h"
#include "mainform.h"
#include "msgbox.h"
#include "ins.h"

void P4PTask::Run() {
  if (file_id.Contains("Rigaku")) {
    TMainForm *mf = TGlXApp::GetMainForm();
    olxstr cmd = "spy.xplain.exists()";
    mf->processFunction(cmd, __OlxSrcInfo);
    if (cmd.IsBool() && cmd.ToBool()) {
      olxstr opt =
        TBasicApp::GetInstance().GetOptions().FindValue("run_rigaku_xplain");
      bool run_xplain=false;
      if (opt.IsEmpty()) {
        olxstr rv = TdlgMsgBox::Execute(mf, "Would you like to run "
          "Rigaku XPlain for the space group determination?",
          "Rigaku P4P file import",
          "Do this for all Rigaku P4P files",
          wxYES|wxNO|wxICON_QUESTION, true);
        run_xplain = rv.Contains('Y');
        if (rv.Containsi('R'))
          mf->UpdateUserOptions("run_rigaku_xplain", run_xplain);
      }
      else {
        run_xplain = opt.ToBool();
      }
      if (run_xplain) {
        olxstr loaded_fn = TXApp::GetInstance().XFile().GetFileName();
        olxstr exts[] = {"ins", "res", "cif"};
        bool cell_src_exists = false;
        for (size_t i=0; i < sizeof(exts)/sizeof(exts[0]); i++) {
          olxstr n = TEFile::ChangeFileExt(loaded_fn, exts[i]);
          if (TEFile::Exists(n)) {
            cell_src_exists = true;
            break;
          }
        }
        if (!cell_src_exists) {
          TIns ins;
          ins.Adopt(TXApp::GetInstance().XFile(), 0);
          ins.SaveForSolution(TEFile::ChangeFileExt(loaded_fn, "ins"), "TREF",
            "Imported by Olex2");
        }
        olxstr fn = "spy.xplain.run(false, true)";
        mf->processFunction(fn);
        if (fn.IsBool() && fn.ToBool()) {
          mf->processMacro("reap", __OlxSrcInfo);
        }
        else {
          TBasicApp::NewLogEntry() << "Failed to run XPlain";
        }
      }
    }
  }
}

void CellChangeTask::Run() {
  TMainForm *mf = TGlXApp::GetMainForm();
  if (hklsrc != TXApp::GetInstance().XFile().GetRM().GetHKLSource()) {
    return;
  }
  TXFile &xf = TXApp::GetInstance().XFile();
  // skip if HKLF matrix is set
  if (!xf.GetRM().GetHKLF_mat().IsI()) {
    return;
  }
  TAsymmUnit &au = xf.GetAsymmUnit();
  olxstr o, n;
  double diff = 0;
  for (size_t i=0; i < 3; i++) {
    o << ' ' << TEValueD(au.GetAxes()[i], au.GetAxisEsds()[i]).ToString() <<
      ' ' << TEValueD(au.GetAngles()[i], au.GetAngleEsds()[i]).ToString();
    n << ' ' << TEValueD(axes[i], axis_esd[i]).ToString() <<
      ' ' << TEValueD(angles[i], angle_esd[i]).ToString();
    diff += olx_abs(au.GetAxes()[i]- axes[i]);
  }
  if (diff / 3 > 1) {
    TBasicApp::NewLogEntry(logError) <<
      "There is a new cell in the HKL file but it differs significantly from "
      "the current one - please update manually";
    return;
  }
  bool use = TBasicApp::GetInstance().GetOptions().FindValue(
    "use_hkl_cell", TrueString()).ToBool();
  if (!use && o != n) {
    olxstr msg = "Cell parameters in your HKL file differ from currently "
      "used.\nCurrent:";
    msg << o << "\nNew:" << n << "\nWould you like to update them?";
    olxstr rv = TdlgMsgBox::Execute(mf, msg,
      "Different cells",
      "Remember my decision",
      wxYES|wxNO|wxICON_QUESTION, true);
    use = rv.Contains('Y');
    if (rv.Containsi('R')) {
      mf->UpdateUserOptions("use_hkl_cell", use);
    }
  }
  if (use) {
   TAsymmUnit &au1 = xf.LastLoader()->GetAsymmUnit();
    au1.GetAxes() = au.GetAxes() = axes;
    au1.GetAngles() = au.GetAngles() = angles;
    au1.GetAxisEsds() = au.GetAxisEsds() = axis_esd;
    au1.GetAngleEsds() = au.GetAngleEsds() = angle_esd;
    TBasicApp::NewLogEntry() <<
      "Updating cell parameters with value from HKL file";
  }
}
#endif // Harmony task runner / original desktop tasks
