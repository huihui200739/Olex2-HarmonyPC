/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

// Original desktop Init body, included by glmaterial.cpp only.
#ifndef __olx_gl_material_legacy_init_H
#define __olx_gl_material_legacy_init_H
void TGlMaterial::Init(bool skip) const {
  if (skip) {
    return;
  }
  if (Flags & sglmTransparent) {
    if (!glIsEnabled(GL_ALPHA_TEST)) {
      olx_gl::enable(GL_ALPHA_TEST);
      olx_gl::enable(GL_POINT_SMOOTH);
      olx_gl::enable(GL_BLEND);
      olx_gl::enable(GL_CULL_FACE);
    }
  }
  else {
    if (glIsEnabled(GL_ALPHA_TEST)) {
      olx_gl::disable(GL_ALPHA_TEST);
      olx_gl::disable(GL_POINT_SMOOTH);
      olx_gl::disable(GL_BLEND);
      olx_gl::disable(GL_CULL_FACE);
    }
  }
  /*
  if (Flags & sglmLighting) {
    olx_gl::enable(GL_LIGHTING);
  }
  else {
    olx_gl::disable(GL_LIGHTING);
  }
  */
  if ((Flags & sglmColorMat) == 0) {
    olx_gl::disable(GL_COLOR_MATERIAL);
    olx_gl::material(GL_FRONT, GL_DIFFUSE,
      ((Flags & sglmDiffuseF) != 0 ? DiffuseF : BlackColor).Data());
    olx_gl::material(GL_BACK, GL_DIFFUSE,
      ((Flags & sglmDiffuseB) != 0 ? DiffuseB : BlackColor).Data());
    olx_gl::material(GL_FRONT, GL_AMBIENT,
      ((Flags & sglmAmbientF) != 0 ? AmbientF : BlackColor).Data());
    olx_gl::material(GL_BACK, GL_AMBIENT,
      ((Flags & sglmAmbientB) != 0 ? AmbientB : BlackColor).Data());
    olx_gl::material(GL_FRONT, GL_SPECULAR,
      ((Flags & sglmSpecularF) != 0 ? SpecularF : BlackColor).Data());
    olx_gl::material(GL_BACK, GL_SPECULAR,
      ((Flags & sglmSpecularB) != 0 ? SpecularB : BlackColor).Data());
    olx_gl::material(GL_FRONT, GL_EMISSION,
      ((Flags & sglmEmissionF) != 0 ? EmissionF : BlackColor).Data());
    olx_gl::material(GL_BACK, GL_EMISSION,
      ((Flags & sglmEmissionB) != 0 ? EmissionB : BlackColor).Data());
    olx_gl::material(GL_FRONT, GL_SHININESS,
      ((Flags & sglmShininessF) != 0 ? ShininessF : 0));
    olx_gl::material(GL_BACK, GL_SHININESS,
      ((Flags & sglmShininessB) != 0 ? ShininessB : 0));
  }
  else {
    olx_gl::enable(GL_COLOR_MATERIAL);
    if (Flags & sglmAmbientF) {
      olx_gl::colorMaterial(GL_FRONT, GL_AMBIENT);
      olx_gl::color(AmbientF.Data());
    }
    if (Flags & sglmSpecularF) {
      olx_gl::colorMaterial(GL_FRONT, GL_SPECULAR);
      olx_gl::color(SpecularF.Data());
    }
    if (Flags & sglmSpecularB) {
      olx_gl::colorMaterial(GL_BACK, GL_SPECULAR);
      olx_gl::color(SpecularB.Data());
    }
    if (Flags & sglmShininessF) {
      olx_gl::material(GL_FRONT, GL_SHININESS, ShininessF);
    }
    if (Flags & sglmShininessB) {
      olx_gl::material(GL_BACK, GL_SHININESS, ShininessB);
    }
    olx_gl::colorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  }
}
#endif
