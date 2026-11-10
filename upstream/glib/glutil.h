/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#ifndef __olx_glutil_H
#define __olx_glutil_H
#include "glalg.h"
#include "bapp.h"
#include "sortedlist.h"
#include "spheresubmit.h"
#include "primitivesubmit.h"
#include <type_traits>
BeginGlNamespace()

// Retains the existing immediate-mode desktop backend and public API.
struct SphereDesktopSink {
  void BeginTriangles() { olx_gl::begin(GL_TRIANGLES); }
  template <class V> void Normal(const V& v) { olx_gl::normal(v); }
  template <class V> void Vertex(const V& v) { olx_gl::vertex(v); }
  template <class X, class Y, class Z> void Normal(X x, Y y, Z z) {
    typedef typename std::common_type<X,Y,Z>::type N;
    olx_gl::normal(N(x),N(y),N(z));
  }
  template <class X, class Y, class Z> void Vertex(X x, Y y, Z z) {
    typedef typename std::common_type<X,Y,Z>::type N;
    olx_gl::vertex(N(x),N(y),N(z));
  }
  void Begin(PrimitiveTopology topology) {
    switch (topology) {
      case PrimitiveTopology::Triangles: olx_gl::begin(GL_TRIANGLES); break;
      case PrimitiveTopology::Quads: olx_gl::begin(GL_QUADS); break;
      case PrimitiveTopology::QuadStrip: olx_gl::begin(GL_QUAD_STRIP); break;
      case PrimitiveTopology::ConvexPolygon: olx_gl::begin(GL_POLYGON); break;
    }
  }
  void End() { olx_gl::end(); }
};

template <typename float_type, class FaceProvider> class GlSphereEx :
  public OlxSphere<float_type,FaceProvider>
{
  typedef TVector3<float_type> vec_t;
public:
  static void Render(float_type rad, size_t ext)  {
    TTypeList<vec_t> vecs;
    TTypeList<IndexTriangle> triags;
    TArrayList<vec_t> norms;
    OlxSphere<float_type,FaceProvider>::Generate(rad, ext, vecs, triags, norms);
    Render(vecs, triags, norms);
  }
  static void Render(const TTypeList<vec_t>& vecs,
    const TTypeList<IndexTriangle>& triags,
    const TArrayList<vec_t>& norms)
  {
    SphereDesktopSink sink;
    SphereTriangleSubmission<float_type>::Render(vecs, triags, norms, sink);
  }

  static void RenderEx(float_type rad, size_t ext,
    const vec_t& f_mask, const vec_t& t_mask)
  {
    TTypeList<vec_t> vecs;
    TTypeList<IndexTriangle> triags;
    TArrayList<vec_t> norms;
    OlxSphere<float_type,FaceProvider>::Generate(rad, ext, vecs, triags, norms);
    RenderEx(vecs, triags, norms, f_mask, t_mask);
  }
  static void RenderEx(const TTypeList<vec_t>& vecs,
    const TTypeList<IndexTriangle>& triags,
    const TArrayList<vec_t>& norms,
    const vec_t& f_mask, const vec_t& t_mask)
  {
    SphereDesktopSink sink;
    SphereTriangleSubmission<float_type>::RenderEx(
      vecs, triags, norms, f_mask, t_mask, sink);
  }
  void RenderDisks(float_type rad, size_t t, float_type disk_s)  {
    SphereDesktopSink sink;
    DiskPrimitiveSubmission<float_type>::RenderDisks(rad,t,disk_s,sink);
  }
  void RenderRims(float_type rad, size_t t, float_type disk_s)  {
    SphereDesktopSink sink;
    DiskPrimitiveSubmission<float_type>::RenderRims(rad,t,disk_s,sink);
  }
};

struct GlTorus {
  //reference: www.opengl.org/resources/code/samples/redbook/torus.c
  // tr - tube radius, r - torus radius
  static void Render(double tr, double r, size_t sc, size_t tsc) {
    SphereDesktopSink sink;
    TorusPrimitiveSubmission::Render(tr,r,sc,tsc,sink);
  }
};
EndGlNamespace()
#endif
