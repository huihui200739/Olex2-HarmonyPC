/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

// The vertex/normal/color branch of TGlPrimitive::Draw(sgloTriangles).
// Begin/end, material, transforms, clipping, texture and display lists remain
// the caller's responsibility. This header does not implement desktop OpenGL.
#ifndef __olx_primitive_triangle_submit_H
#define __olx_primitive_triangle_submit_H
#include <cstddef>
#include "glnamespace.h"
BeginGlNamespace()
template <class V, class N, class C, class Sink>
void SubmitPrimitiveTriangles(const V& Vertices, const N& Normals,
  const C& Colors, Sink& sink)
{
    if (Normals.IsEmpty()) {
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
    else {
      if (Colors.IsEmpty() || sink.IsSelecting()) {
        if (Normals.Count() == Vertices.Count()) {  //+normal
          for (size_t i = 0; i < Normals.Count(); i++) {
            sink.SetNormal(Normals[i]);
            sink.DrawVertex(Vertices[i]);
          }
        }
        else if (Normals.Count() * 3 == Vertices.Count()) {  //+normal
          for (size_t i = 0; i < Normals.Count(); i++) {
            sink.SetNormal(Normals[i]);
            sink.DrawVertex3(i * 3);
          }
        }
      }
      else if (Colors.Count() == Vertices.Count()) {
        if (Normals.Count() == Vertices.Count()) {  //+normal
          for (size_t i = 0; i < Normals.Count(); i++) {
            sink.SetNormal(Normals[i]);
            sink.DrawVertex(Vertices[i], Colors[i]);
          }
        }
        else if (Normals.Count() * 3 == Vertices.Count()) {  //+normal
          for (size_t i = 0; i < Normals.Count(); i++) {
            sink.SetNormal(Normals[i]);
            sink.DrawVertex3c(i * 3);
          }
        }
      }
    }
}
EndGlNamespace()
#endif
