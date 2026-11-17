/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#ifndef __olx_zip_url_H
#define __olx_zip_url_H
#include "efile.h"

// ZIP URI operations do not depend on an archive stream or GUI toolkit.
struct TZipEntry {
  olxstr ZipName;
  olxstr EntryName;
};

class TZipUrl {
public:
  static const olxstr& ZipUrlSignature_() {
    static olxstr sig = "@zip:";
    return sig;
  }
  static bool IsValidFileName(const olxstr& FN);
  static bool IsZipFile(const olxstr& FN);
  static olxstr ExtractZipName(const olxstr& FN);
  static olxstr ExtractZipEntryName(const olxstr& FN);
  static bool SplitZipUrl(const olxstr& fullName, TZipEntry& ZE);
  static olxstr ComposeFileName(const olxstr& ZipFileNameA, const olxstr& FNA);
};

inline bool TZipUrl::IsValidFileName(const olxstr &FN) {
  size_t zi = FN.IndexOf(ZipUrlSignature_());
  if (zi != InvalidIndex) {
    if (TEFile::Exists(ExtractZipName(FN))) {
      return true;
    }
    return false;
  }
  return TEFile::Exists(FN);
}
inline bool TZipUrl::IsZipFile(const olxstr &FN) {
  size_t ind = FN.IndexOf(ZipUrlSignature_());
  return ind != InvalidIndex && ind != 0;
}
inline olxstr TZipUrl::ExtractZipName(const olxstr &FN) {
  size_t zi = FN.IndexOf(ZipUrlSignature_());
  if (zi != InvalidIndex && zi > 0) {
    return FN.SubStringTo(zi);
  }
  return EmptyString();
}
inline olxstr TZipUrl::ExtractZipEntryName(const olxstr &FN) {
  size_t zi = FN.IndexOf(ZipUrlSignature_());
  if (zi != InvalidIndex && zi > 0) {
    return FN.SubStringFrom(zi + ZipUrlSignature_().Length());
  }
  return EmptyString();
}
inline bool TZipUrl::SplitZipUrl(const olxstr& fullName, TZipEntry& ZE) {
  size_t zi = fullName.IndexOf(ZipUrlSignature_());
  if (zi == InvalidIndex) {
    return false;
  }
  ZE.ZipName = fullName.SubStringTo(zi);
  ZE.ZipName = TEFile::UnixPath(ZE.ZipName);
  ZE.EntryName = fullName.SubStringFrom(zi + ZipUrlSignature_().Length());
  ZE.EntryName = TEFile::UnixPath(ZE.EntryName);
  return true;
}
inline olxstr TZipUrl::ComposeFileName(const olxstr& ZipFileNameA,
  const olxstr& FNA)
{
  size_t zi = ZipFileNameA.IndexOf(ZipUrlSignature_());
  if (zi == InvalidIndex) {
    return FNA;
  }
  olxstr ZipFileName = TEFile::WinPath(ZipFileNameA),
    FN = TEFile::WinPath(FNA);
  olxstr zipPart = ZipFileName.SubStringTo(zi + ZipUrlSignature_().Length());
  olxstr zipPath = TEFile::ExtractFilePath(zipPart);
  TEFile::AddPathDelimeterI(zipPath);
  if (FN.StartsFrom(zipPath)) {
    return zipPart + TEFile::UnixPath(FN.SubStringFrom(zipPath.Length()));
  }
  return FNA;
}

#endif
