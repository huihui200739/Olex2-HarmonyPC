/******************************************************************************
 * Copyright (c) 2004-2026 O. Dolomanov, OlexSys                               *
 *                                                                             *
 * This file is part of the OlexSys Development Framework.                     *
 *                                                                             *
 * This source file is distributed under the terms of the licence located in   *
 * the root folder.                                                            *
 ******************************************************************************/

#ifndef __olx_gl_namespace_H
#define __olx_gl_namespace_H

#define BeginGlNamespace()  namespace glObj {
#define EndGlNamespace()  };\
  using namespace glObj;
#define UseGlNamespace()  using namespace glObj;
#define GlobalGlFunction( fun )     glObj::fun

#endif
