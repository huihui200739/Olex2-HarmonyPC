/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

// Backend-independent triangle submission extracted from GlSphereEx.
// The sink owns the batch lifetime; no desktop GL headers or GLU are required.
#ifndef __olx_sphere_submit_H
#define __olx_sphere_submit_H
#include "esphere.h"
namespace glObj {
template <typename float_type> struct SphereTriangleSubmission {
  typedef TVector3<float_type> vec_t;
  template <class Sink>
  static void Render(const TTypeList<vec_t>& vecs,
    const TTypeList<IndexTriangle>& triags,
    const TArrayList<vec_t>& norms, Sink& sink)
  {
    const size_t tc = triags.Count();
    sink.BeginTriangles();
    for (size_t i = 0; i < tc; i++) {
      const IndexTriangle& t = triags[i];
      for (size_t j = 0; j < 3; j++) {
        sink.Normal(norms[t.vertices[j]]);
        sink.Vertex(vecs[t.vertices[j]]);
      }
    }
    sink.End();
  }
  template <class Sink>
  static void RenderEx(const TTypeList<vec_t>& vecs,
    const TTypeList<IndexTriangle>& triags,
    const TArrayList<vec_t>& norms,
    const vec_t& f_mask, const vec_t& t_mask, Sink& sink)
  {
    const size_t tc = triags.Count();
    sink.BeginTriangles();
    for (size_t i = 0; i < tc; i++) {
      const IndexTriangle& t = triags[i];
      if (vec_t::IsInRangeInc(vecs[t.vertices[0]], f_mask, t_mask) &&
          vec_t::IsInRangeInc(vecs[t.vertices[1]], f_mask, t_mask) &&
          vec_t::IsInRangeInc(vecs[t.vertices[2]], f_mask, t_mask))
        continue;
      for (size_t j = 0; j < 3; j++) {
        sink.Normal(norms[t.vertices[j]]);
        sink.Vertex(vecs[t.vertices[j]]);
      }
    }
    sink.End();
  }
};
}
#endif
