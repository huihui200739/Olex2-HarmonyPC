/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

// Original glutil helper loops, with only the output backend parameterized.
// Geometry generation and legacy batch boundaries are intentionally retained.
#ifndef __olx_primitive_submit_H
#define __olx_primitive_submit_H
#include "esphere.h"
namespace glObj {
enum class PrimitiveTopology { Triangles, Quads, QuadStrip, ConvexPolygon };
template <typename float_type> struct DiskPrimitiveSubmission {
  typedef TVector3<float_type> vec_t;
  template <class Sink>
  static void RenderDisks(float_type rad, size_t t, float_type disk_s, Sink& sink) {
    vec_t sf(rad, 0, 0);
    float_type sa, sma, ca, cma;
    olx_sincos(2*M_PI/t, &sa, &ca);
    olx_sincos(-2*M_PI/t, &sma, &cma);
    sink.Begin(PrimitiveTopology::ConvexPolygon);
    sink.Normal(0, 0, -1);
    for( int i=0; i <= t; i++ )  {
      sink.Vertex(sf[0], sf[1], -disk_s/2);
      if( (i+1) > t )  break;
      float_type x = sf[0];
      sf[0] = (x*ca + sf[1]*sa);
      sf[1] = (sf[1]*ca - x*sa);
    }
    sink.End();
    sf = vec_t(rad, 0, 0);
    sink.Begin(PrimitiveTopology::ConvexPolygon);
    sink.Normal(0, 0, 1);
    for( size_t i=0; i <= t; i++ )  {
      sink.Vertex(sf[0], sf[1], disk_s/2);
      if( (i+1) > t )  break;
      float_type x = sf[0];
      sf[0] = (x*cma + sf[1]*sma);
      sf[1] = (sf[1]*cma - x*sma);
    }
    sink.End();

    sf = vec_t(rad, 0, 0);
    sink.Begin(PrimitiveTopology::ConvexPolygon);
    sink.Normal(0, -1, 0);
    for( size_t i=0; i <= t; i++ )  {
      sink.Vertex(sf[1], -disk_s/2, sf[0]);
      if( (i+1) > t )  break;
      float_type x = sf[0];
      sf[0] = (x*ca + sf[1]*sa);
      sf[1] = (sf[1]*ca - x*sa);
    }
    sink.End();
    sf = vec_t(rad, 0, 0);
    sink.Begin(PrimitiveTopology::ConvexPolygon);
    sink.Normal(0, 1, 0);
    for( size_t i=0; i <= t; i++ )  {
      sink.Vertex(sf[1], disk_s/2, sf[0]);
      if( (i+1) > t )  break;
      float_type x = sf[0];
      sf[0] = (x*cma + sf[1]*sma);
      sf[1] = (sf[1]*cma - x*sma);
    }
    sink.End();

    sf = vec_t(rad, 0, 0);
    sink.Begin(PrimitiveTopology::ConvexPolygon);
    sink.Normal(-1, 0, 0);
    for( size_t i=0; i <= t; i++ )  {
      sink.Vertex(-disk_s/2, sf[0], sf[1]);
      float_type x = sf[0];
      sf[0] = (x*ca + sf[1]*sa);
      sf[1] = (sf[1]*ca - x*sa);
    }
    sink.End();
    sf = vec_t(rad, 0, 0);
    sink.Begin(PrimitiveTopology::ConvexPolygon);
    sink.Normal(1, 0, 0);
    for( size_t i=0; i <= t; i++ )  {
      sink.Vertex(disk_s/2, sf[0], sf[1]);
      float_type x = sf[0];
      sf[0] = (x*cma + sf[1]*sma);
      sf[1] = (sf[1]*cma - x*sma);
    }
    sink.End();
    }
  template <class Sink>
  static void RenderRims(float_type rad, size_t t, float_type disk_s, Sink& sink) {
    vec_t sf(rad, 0, 0), pv;
    float_type sa, ca;
    olx_sincos(2*M_PI/t, &sa, &ca);
    sink.Begin(PrimitiveTopology::Quads);
    for( size_t i=0; i <= t; i++ )  {
      sink.Normal(sf[0], sf[1], 0.0f);
      sink.Vertex(sf[0], sf[1], -disk_s/2);
      sink.Vertex(sf[0], sf[1], +disk_s/2);
      if( (i+1) > t )  break;
      float_type x = sf[0];
      sf[0] = (x*ca + sf[1]*sa);
      sf[1] = (sf[1]*ca - x*sa);
      sink.Normal(sf[0], sf[1], 0.0f);
      sink.Vertex(sf[0], sf[1], +disk_s/2);
      sink.Vertex(sf[0], sf[1], -disk_s/2);
    }
    sink.End();

    sf = vec_t(rad, 0, 0);
    sink.Begin(PrimitiveTopology::Quads);
    for( size_t i=0; i <= t; i++ )  {
      sink.Normal(sf[1], 0.0f, sf[0]);
      sink.Vertex(sf[1], -disk_s/2, sf[0]);
      sink.Vertex(sf[1], +disk_s/2, sf[0]);
      if( (i+1) > t )  break;
      float_type x = sf[0];
      sf[0] = (x*ca + sf[1]*sa);
      sf[1] = (sf[1]*ca - x*sa);
      sink.Normal(sf[1], 0.0f, sf[0]);
      sink.Vertex(sf[1], +disk_s/2, sf[0]);
      sink.Vertex(sf[1], -disk_s/2, sf[0]);
    }
    sink.End();

    sf = vec_t(rad, 0, 0);
    sink.Begin(PrimitiveTopology::Quads);
    for( size_t i=0; i <= t; i++ )  {
      sink.Normal(0.0f, sf[0], sf[1]);
      sink.Vertex(-disk_s/2, sf[0], sf[1]);
      sink.Vertex(+disk_s/2, sf[0], sf[1]);
      float_type x = sf[0];
      sf[0] = (x*ca + sf[1]*sa);
      sf[1] = (sf[1]*ca - x*sa);
      sink.Normal(0.0f, sf[0], sf[1]);
      sink.Vertex(+disk_s/2, sf[0], sf[1]);
      sink.Vertex(-disk_s/2, sf[0], sf[1]);
    }
    sink.End();
    }
};
struct TorusPrimitiveSubmission {
  template <class Sink>
  static void Render(double tr, double r, size_t sc, size_t tsc, Sink& sink) {
    static const double tpi = 2 * M_PI;
    for (size_t i = 0; i < sc; i++) {
      sink.Begin(PrimitiveTopology::QuadStrip);
      for (size_t j = 0; j <= tsc; j++) {
        for (int k = 1; k >= 0; k--) {
          double s = (i + k) % sc + 0.5;
          double t = static_cast<double>(j % tsc);
          double a = cos(s * tpi / sc),
            b = cos(t * tpi / tsc),
            c = sin(t * tpi / tsc),
            d = sin(s * tpi / sc);
          sink.Normal(a * b, a * c, d);
          sink.Vertex((r + tr * a) * b, (r + tr * a) * c, tr * d);
        }
      }
      sink.End();
    }
    }
};
}
#endif
