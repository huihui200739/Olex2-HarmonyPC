/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

// Exact sgloLines body, excluding begin/end. Material, lighting, clipping,
// transforms and display lists remain the caller's responsibility.
#ifndef __olx_primitive_line_submit_H
#define __olx_primitive_line_submit_H
#include <cstddef>
#include "glnamespace.h"
BeginGlNamespace()
template <class V, class C, class P, class Sink>
void SubmitPrimitiveLines(const V& Vertices, const C& Colors,
  const P& Params, Sink& sink)
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
    else if (Colors.Count() * 2 == Vertices.Count()) {
      for (size_t i = 0; i < Colors.Count(); i++) {
        sink.SetColor(Colors[i]);
        sink.DrawVertex2(i * 2);
      }
    }
    if (LW != 0) {
      sink.SetLineWidth(LW);
    }
}
EndGlNamespace()
#endif
