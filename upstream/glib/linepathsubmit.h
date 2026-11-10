/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

// Exact original strip/loop bodies; caller retains GL begin/end and state.
#ifndef __olx_primitive_line_path_submit_H
#define __olx_primitive_line_path_submit_H
#include <cstddef>
#include "glnamespace.h"
BeginGlNamespace()
template<class V,class C,class P,class Sink>
void SubmitPrimitiveLineStrip(const V& Vertices,const C& Colors,const P& Params,Sink& sink)
{
    float LW = 0;
    if (Params[0] != 1) {
      sink.GetLineWidth(&LW);
      sink.SetLineWidth(Params[0] * LW);
    }
    for (size_t i = 0; i < Vertices.Count(); i++) {
      if (Colors.Count() == Vertices.Count()) {
        sink.SetColor(Colors[i]);
      }
      sink.DrawVertex(Vertices[i]);
    }
    if (LW != 0) {
      sink.SetLineWidth(LW);
    }
}
template<class V,class C,class P,class Sink>
void SubmitPrimitiveLineLoop(const V& Vertices,const C& Colors,const P& Params,Sink& sink)
{
    float LW = 0;
    if (Params[0] != 1) {
      sink.GetLineWidth(&LW);
      sink.SetLineWidth(Params[0] * LW);
    }
    if (Colors.IsEmpty() || sink.IsSelecting()) {
      for (size_t i = 0; i < Vertices.Count(); i++) {
        sink.DrawVertex(Vertices[i]);
      }
    }
    else if (Colors.Count() == Vertices.Count()) {
      for (size_t i = 0; i < Vertices.Count(); i++) {
        sink.DrawVertex(Vertices[i], Colors[i]);
      }
    }
    if (LW != 0) {
      sink.SetLineWidth(LW);
    }
}
EndGlNamespace()
#endif
