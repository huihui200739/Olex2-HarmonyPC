#pragma once
#include "original_renderer_host.h"
#include "original_document_framing.h"
#include "platform_window_scene.h"
#include "gxapp.h"
#include <cstdint>
#include <string>
namespace olex2::harmony {
// Last successfully applied native fit, not a current-frame/GPU assertion.
struct OriginalDocumentFraming {
  bool applied=false;
  int width=0,height=0;
  size_t worldBoxCount=0;
  uint64_t revision=0,contextGeneration=0;
  std::string action;
  double yaw=0,pitch=0,defaultDistance=0,originalZoom=0,zoom=0,eyeOriginZ=0;
  framing::Bounds bounds;
  framing::Point centre{};
  framing::Matrix rotation{};
  framing::Projection projection{};
};
// Per-original-owner state. No second application singleton and no user URI.
struct OriginalDocumentState {
  bool loaded=false, compatibilityReady=false,contentObserved=false;
  uint64_t revision=0, frames=0, contextGeneration=0;
  uint64_t selectionSequence=0,originalPickingCalls=0;
  bool pickHit=false;
  int lastPickPixelX=-1,lastPickPixelY=-1;
  std::string selectionAction,lastPickedCollection;
  size_t byteCount=0, exportByteCount=0;
  size_t sampleWidth=0,sampleHeight=0,nonBackgroundPixels=0;
  size_t sampleReadPixels=0,sampleTileCount=0,centralSampleWidth=0,centralSampleHeight=0,centralSampleNonBackgroundPixels=0;
  std::string name, format, sha256, directory;
  TEBasis centredBasis;
  OriginalDocumentFraming lastFraming;
};
// Caller holds application mutex and original interpreter guard. Reads only the
// currently loaded owner's exclusive stage; never invokes its saver or GL.
std::string ReadOriginalDocumentSourceLocked(const TGXApp&, const OriginalDocumentState&);
// Caller holds application mutex and original interpreter guard. The same
// isolated GL context must survive between requests until explicit Close.
std::string RunOriginalDocumentLocked(const Olex2LegacyHost&, TGXApp&,
  PlatformWindowScene&, PlatformSceneLifetime&, OriginalDocumentState&);
}
