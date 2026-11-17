/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

// Backend-neutral command form of the original Init control flow.
// Checked against the pinned original by scripts/check-source.py.
#ifndef __olx_gl_material_commands_H
#define __olx_gl_material_commands_H
#include "glmaterial.h"
BeginGlNamespace()
enum class MaterialCapability { AlphaTest, PointSmooth, Blend, CullFace, ColorMaterial, Lighting };
enum class MaterialFace { Front, Back, Both };
enum class MaterialField { Ambient, Diffuse, Specular, Emission, Shininess, AmbientAndDiffuse };

template <class Sink>
void ApplyMaterialCommands(const TGlMaterial& value, Sink& api, bool skip) {
  if (skip) {
    return;
  }
  if (value.GetFlags() & sglmTransparent) {
    if (!api.IsEnabled(MaterialCapability::AlphaTest)) {
      api.enable(MaterialCapability::AlphaTest);
      api.enable(MaterialCapability::PointSmooth);
      api.enable(MaterialCapability::Blend);
      api.enable(MaterialCapability::CullFace);
    }
  }
  else {
    if (api.IsEnabled(MaterialCapability::AlphaTest)) {
      api.disable(MaterialCapability::AlphaTest);
      api.disable(MaterialCapability::PointSmooth);
      api.disable(MaterialCapability::Blend);
      api.disable(MaterialCapability::CullFace);
    }
  }
  /*
  if (value.GetFlags() & sglmLighting) {
    api.enable(MaterialCapability::Lighting);
  }
  else {
    api.disable(MaterialCapability::Lighting);
  }
  */
  if ((value.GetFlags() & sglmColorMat) == 0) {
    api.disable(MaterialCapability::ColorMaterial);
    api.material(MaterialFace::Front, MaterialField::Diffuse,
      ((value.GetFlags() & sglmDiffuseF) != 0 ? value.DiffuseF : BlackColor).Data());
    api.material(MaterialFace::Back, MaterialField::Diffuse,
      ((value.GetFlags() & sglmDiffuseB) != 0 ? value.DiffuseB : BlackColor).Data());
    api.material(MaterialFace::Front, MaterialField::Ambient,
      ((value.GetFlags() & sglmAmbientF) != 0 ? value.AmbientF : BlackColor).Data());
    api.material(MaterialFace::Back, MaterialField::Ambient,
      ((value.GetFlags() & sglmAmbientB) != 0 ? value.AmbientB : BlackColor).Data());
    api.material(MaterialFace::Front, MaterialField::Specular,
      ((value.GetFlags() & sglmSpecularF) != 0 ? value.SpecularF : BlackColor).Data());
    api.material(MaterialFace::Back, MaterialField::Specular,
      ((value.GetFlags() & sglmSpecularB) != 0 ? value.SpecularB : BlackColor).Data());
    api.material(MaterialFace::Front, MaterialField::Emission,
      ((value.GetFlags() & sglmEmissionF) != 0 ? value.EmissionF : BlackColor).Data());
    api.material(MaterialFace::Back, MaterialField::Emission,
      ((value.GetFlags() & sglmEmissionB) != 0 ? value.EmissionB : BlackColor).Data());
    api.material(MaterialFace::Front, MaterialField::Shininess,
      ((value.GetFlags() & sglmShininessF) != 0 ? value.ShininessF : 0));
    api.material(MaterialFace::Back, MaterialField::Shininess,
      ((value.GetFlags() & sglmShininessB) != 0 ? value.ShininessB : 0));
  }
  else {
    api.enable(MaterialCapability::ColorMaterial);
    if (value.GetFlags() & sglmAmbientF) {
      api.colorMaterial(MaterialFace::Front, MaterialField::Ambient);
      api.color(value.AmbientF.Data());
    }
    if (value.GetFlags() & sglmSpecularF) {
      api.colorMaterial(MaterialFace::Front, MaterialField::Specular);
      api.color(value.SpecularF.Data());
    }
    if (value.GetFlags() & sglmSpecularB) {
      api.colorMaterial(MaterialFace::Back, MaterialField::Specular);
      api.color(value.SpecularB.Data());
    }
    if (value.GetFlags() & sglmShininessF) {
      api.material(MaterialFace::Front, MaterialField::Shininess, value.ShininessF);
    }
    if (value.GetFlags() & sglmShininessB) {
      api.material(MaterialFace::Back, MaterialField::Shininess, value.ShininessB);
    }
    api.colorMaterial(MaterialFace::Both, MaterialField::AmbientAndDiffuse);
  }
}

EndGlNamespace()
#endif
