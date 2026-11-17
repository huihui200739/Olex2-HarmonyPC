/******************************************************************************
* Copyright (c) 2004-2011 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#include "exception.h"
#include "library.h"
#include "olxth.h"
#include "glpixels.h"
#include "edict.h"

namespace olx_nui {

class NUIInitFaieldEx : public TBasicException {
public:
  NUIInitFaieldEx(const olxstr &location, const olxstr &msg=EmptyString())
    : TBasicException(location, msg) {}
  const char* GetNiceName() const {  return "NUI initialisation failed";  }
  IOlxObject* Replicate() const {  return new NUIInitFaieldEx(*this);  }
};

class INUI : public IOlxObject {
public:
  static const short
    processVideo    = 0x0001,
    processSkeleton = 0x0002,
    processDepth    = 0x0004;
  virtual void InitProcessing(short flags) = 0;
  virtual void DoProcessing() = 0;
};

INUI *Initialise();

#if defined __WIN32__ && !defined(_WIN64) && defined(__OLX_USE_NUI__)
#include <MSR_NuiApi.h>

class Skeleton : public AGDrawObject {
protected:
  void DrawQuad(const vec3d &p1, const vec3d &p2,
    const vec3d &p3, const vec3d &p4);
  void DrawLimp(const vec3d &shoulder, const vec3d &wrist,
    const vec3d &body, const vec3d &p_shoulder, const vec3d &p_elbow,
    const vec3d &p_wrist, bool cw);
public:
  Skeleton(TGlRenderer& Renderer, const olxstr& collectionName)
  : AGDrawObject(Renderer, collectionName),
    points(NUI_SKELETON_COUNT)
  {}
  void Create(const olxstr& cName=EmptyString());
  bool Orient(TGlPrimitive& P);
  bool GetDimensions(vec3d& Max, vec3d& Min)  {
    return false;
  }
  TArrayList<vec3d_alist> points;
};

class Kinect : public INUI {
  bool Initialised;
  HANDLE hDepthFrameEvt, hVideoFrameEvt, hSkeletonEvt;
  HANDLE hDepthStream, hVideoStream;
  void throw_failed(const olxstr &src, HRESULT res, const olxstr &msg=EmptyString());
protected:
  class ListenerThread : public AOlxThread {
  Kinect &instance;
  HANDLE events[3];
  public:
    ListenerThread(Kinect &_instance) : instance(_instance)  {
      events[0] = instance.hDepthFrameEvt;
      events[1] = instance.hVideoFrameEvt;
      events[2] = instance.hSkeletonEvt;
    }
    virtual int Run();
  };
  ListenerThread *listener;
  Skeleton *skeleton;
  TGlPixels *pixels;
  olxstr last_command;
  olxstr_dict<olxstr, true> commands;
public:
  Kinect()
  : Initialised(false),
    hDepthFrameEvt(INVALID_HANDLE_VALUE),
    hVideoFrameEvt(INVALID_HANDLE_VALUE),
    hSkeletonEvt(INVALID_HANDLE_VALUE),
    listener(NULL),
    HasVideoFrame(false),
    HasDepthFrame(false),
    HasSkeleton(false),
    skeleton(NULL),
    pixels(NULL)
  {}

  ~Kinect()  {  Finalise();  }

  Kinect* Initialise();
  void Finalise();
  bool HasVideoFrame, HasDepthFrame, HasSkeleton;

  virtual void InitProcessing(short flags);
  virtual void DoProcessing();

  void processVideo();
  void processDepth();
  void processSkeleton();
};
#endif //__WIN32__

// The original skeleton mesh is also useful to render externally supplied
// joint frames on Harmony. Keep the Kinect capture implementation Windows-only,
// but share the original renderer and its Kinect-compatible joint ordering.
#if defined(__OHOS__)
enum {
  NUI_SKELETON_COUNT = 6,
  NUI_SKELETON_POSITION_COUNT = 20,
  NUI_SKELETON_POSITION_HIP_CENTER = 0,
  NUI_SKELETON_POSITION_SPINE = 1,
  NUI_SKELETON_POSITION_SHOULDER_CENTER = 2,
  NUI_SKELETON_POSITION_HEAD = 3,
  NUI_SKELETON_POSITION_SHOULDER_LEFT = 4,
  NUI_SKELETON_POSITION_ELBOW_LEFT = 5,
  NUI_SKELETON_POSITION_WRIST_LEFT = 6,
  NUI_SKELETON_POSITION_HAND_LEFT = 7,
  NUI_SKELETON_POSITION_SHOULDER_RIGHT = 8,
  NUI_SKELETON_POSITION_ELBOW_RIGHT = 9,
  NUI_SKELETON_POSITION_WRIST_RIGHT = 10,
  NUI_SKELETON_POSITION_HAND_RIGHT = 11,
  NUI_SKELETON_POSITION_HIP_LEFT = 12,
  NUI_SKELETON_POSITION_KNEE_LEFT = 13,
  NUI_SKELETON_POSITION_ANKLE_LEFT = 14,
  NUI_SKELETON_POSITION_FOOT_LEFT = 15,
  NUI_SKELETON_POSITION_HIP_RIGHT = 16,
  NUI_SKELETON_POSITION_KNEE_RIGHT = 17,
  NUI_SKELETON_POSITION_ANKLE_RIGHT = 18,
  NUI_SKELETON_POSITION_FOOT_RIGHT = 19
};

class Skeleton : public AGDrawObject {
protected:
  void DrawQuad(const vec3d &p1, const vec3d &p2,
    const vec3d &p3, const vec3d &p4);
  void DrawLimp(const vec3d &shoulder, const vec3d &wrist,
    const vec3d &body, const vec3d &p_shoulder, const vec3d &p_elbow,
    const vec3d &p_wrist, bool cw);
public:
  Skeleton(TGlRenderer& renderer, const olxstr& collectionName)
    : AGDrawObject(renderer, collectionName), points(NUI_SKELETON_COUNT) {}
  void Create(const olxstr& cName=EmptyString());
  bool Orient(TGlPrimitive& primitive);
  bool GetDimensions(vec3d& Max, vec3d& Min) { return false; }
  TArrayList<vec3d_alist> points;
};
#endif // __OHOS__ skeleton renderer
};
