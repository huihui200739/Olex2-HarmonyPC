/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

// Original sgloPolygon submission only. Cull-face/begin/end remain with the
// desktop caller; the target supplies its own explicit rasterization policy.
#ifndef __olx_polygon_submit_H
#define __olx_polygon_submit_H
#include "glnamespace.h"
BeginGlNamespace()
template <class VerticesType,class NormalsType,class ColorsType,class Sink>
void SubmitPrimitivePolygon(const VerticesType& Vertices,const NormalsType& Normals,
    const ColorsType& Colors,Sink& sink) {
    if (Normals.Count() == 1) {
      sink.SetNormal(Normals[0]);
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
}
EndGlNamespace()
#endif
