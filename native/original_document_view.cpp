#include "original_document_view.h"
#include "harmony_main_gui.h"
#include "original_document_framing.h"
#include "eutf8.h"
#include "platform_sha256.h"
#include "original_document_viewport_objects.h"
#include "original_document_visibility.h"
#include "xatom.h"
#include "xbond.h"
#include <optional>
#include "gl4esinit.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cctype>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <exception>
#include <iterator>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace olex2::harmony {
namespace {
constexpr size_t MaximumDocumentBytes=8*1024*1024;
olxstr Text(const std::string& s){return TUtf8::Decode(s.c_str());}
std::string Utf8(const olxstr& s){return TUtf8::Encode(s).c_str();}
std::string SelectedParts(TGXApp& app){
  std::string result;const auto& selection=app.GetRenderer().GetSelection();
  for(size_t i=0;i<selection.Count()&&result.size()<200;++i){
    auto* atom=dynamic_cast<TXAtom*>(const_cast<AGDrawObject*>(&selection[i]));
    if(!atom)continue;
    if(!result.empty())result+=',';
    result+=std::to_string(atom->CAtom().GetPart());
  }
  return result;
}
std::string Quote(const std::string& s){std::ostringstream o;o<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<c;else if(c>=32)o<<c;else o<<"\\u00"<<"0123456789abcdef"[c>>4]<<"0123456789abcdef"[c&15];}o<<'"';return o.str();}
std::string Hash(const std::string& bytes){return PlatformSha256(bytes);}
std::string ViewHash(TGXApp& app){
  const mat3d& m=app.GetRenderer().GetBasis().GetMatrix();std::ostringstream o;
  for(int r=0;r<3;r++)for(int c=0;c<3;c++){if(r||c)o<<',';o<<std::setprecision(9)<<m[r][c];}
  return Hash(o.str()).substr(0,16);
}
std::string CurrentExceptionText(){
  try {throw;} catch(const TExceptionBase& e){return Utf8(e.GetException()->GetFullMessage());}
  catch(const std::exception& e){return e.what();}catch(...){return "Unknown original operation exception";}
}
void CheckGL(const char* step){const auto e=glGetError();if(e)throw std::runtime_error(std::string(step)+" GL error "+std::to_string(e));}
std::string Format(const std::string& name){
  if(name.empty()||name.size()>240||name=="."||name==".."||name.find_first_of("/\\\r\n")!=std::string::npos||name.find('\0')!=std::string::npos)
    throw std::invalid_argument("Document name must be a basename of at most 240 UTF-8 bytes");
  const auto dot=name.rfind('.');std::string ext=dot==std::string::npos?"":name.substr(dot+1);
  for(auto& c:ext)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if(ext!="cif"&&ext!="ins"&&ext!="res"&&ext!="xyz"&&ext!="pdb"&&ext!="mol")throw std::invalid_argument("Only CIF, INS, RES, XYZ, PDB and MOL text documents are supported");
  return ext;
}
class CurrentDirectory {
  std::string path_;
public:
  CurrentDirectory(){
    // App startup cwd may be searchable but not readable as a directory. Do
    // not demand a directory-listing descriptor and never change permissions.
    // Capture the normal POSIX pathname, then restore only if a loader moved it.
    std::vector<char> buffer(256);
    for(;;){
      if(getcwd(buffer.data(),buffer.size())){path_=buffer.data();break;}
      if(errno!=ERANGE||buffer.size()>=65536)throw std::runtime_error("Cannot capture application working directory (errno "+std::to_string(errno)+")");
      buffer.resize(buffer.size()*2);
    }
  }
  void Restore(){
    if(path_.empty())return;
    std::vector<char> current(path_.size()+1);
    if(getcwd(current.data(),current.size())&&path_==current.data()){path_.clear();return;}
    if(chdir(path_.c_str())!=0)throw std::runtime_error("Cannot restore application working directory (errno "+std::to_string(errno)+")");
    path_.clear();
  }
  ~CurrentDirectory(){if(!path_.empty())chdir(path_.c_str());}
};
// Every directory here is returned by mkdtemp in the current app's private
// directory. Only ordinary files created inside that exclusive directory are
// removed. No traversal of system document-provider URIs or unrelated paths.
void RemoveOwnedDirectory(std::string& path) noexcept {
  if(path.empty())return;
  DIR* directory=opendir(path.c_str());if(!directory)return;
  const int fd=dirfd(directory);bool removable=true;
  while(auto* entry=readdir(directory)){
    const std::string name=entry->d_name;if(name=="."||name=="..")continue;
    struct stat st{};
    if(fstatat(fd,name.c_str(),&st,AT_SYMLINK_NOFOLLOW)!=0||!S_ISREG(st.st_mode)||unlinkat(fd,name.c_str(),0)!=0)removable=false;
  }
  closedir(directory);if(removable&&rmdir(path.c_str())==0)path.clear();
}
class OwnedStage {
public:
  std::string directory;
  explicit OwnedStage(const char* root){
    if(!root||!*root)throw std::invalid_argument("Missing private document directory");
    std::string pattern=std::string(root)+"/original-document-XXXXXX";
    std::vector<char> path(pattern.begin(),pattern.end());path.push_back(0);
    if(!mkdtemp(path.data()))throw std::runtime_error("Cannot create exclusive private document directory");
    directory=path.data();
  }
  ~OwnedStage(){RemoveOwnedDirectory(directory);}
  std::string Path(const std::string& name)const{return directory+"/"+name;}
  void Write(const std::string& name,const std::string& bytes){
    const std::string path=Path(name);const int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
    if(fd<0)throw std::runtime_error("Cannot create staged document");
    size_t written=0;while(written<bytes.size()){
      const auto n=write(fd,bytes.data()+written,bytes.size()-written);
      if(n<0&&errno==EINTR)continue;
      if(n<=0){close(fd);throw std::runtime_error("Cannot write staged document");}written+=static_cast<size_t>(n);
    }
    const int flushed=fsync(fd),closed=close(fd);
    if(flushed!=0||closed!=0)throw std::runtime_error("Cannot flush staged document");
  }
};
// Source reads own descriptors throughout and never follow a symlink, even in
// the private directory's ancestors. Failure never repairs paths/permissions.
class SourceDescriptor {
  int fd_;
public:
  explicit SourceDescriptor(int fd):fd_(fd){
    if(fd_<0)throw std::runtime_error("Cannot open original document source safely");
  }
  ~SourceDescriptor(){if(fd_>=0)close(fd_);}
  SourceDescriptor(const SourceDescriptor&)=delete;
  SourceDescriptor& operator=(const SourceDescriptor&)=delete;
  int Get()const{return fd_;}
  void Reset(int fd){
    if(fd<0)throw std::runtime_error("Cannot open original document source directory safely");
    const int previous=fd_;fd_=fd;close(previous);
  }
  int Release(){const int fd=fd_;fd_=-1;return fd;}
};
int OpenSourceDirectory(const std::string& path){
  if(path.empty()||path.front()!='/'||path.find('\0')!=std::string::npos)
    throw std::runtime_error("Original document source directory is invalid");
  // Search-only parent directories need no listing permission on OHOS/Linux.
#if defined(O_PATH)
  constexpr int flags=O_PATH|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC;
#else
  constexpr int flags=O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC;
#endif
  SourceDescriptor directory(open("/",flags));
  size_t offset=1;
  while(offset<path.size()){
    const auto separator=path.find('/',offset);
    const auto component=path.substr(offset,separator==std::string::npos?std::string::npos:separator-offset);
    if(component=="."||component=="..")throw std::runtime_error("Original document source directory contains traversal");
    if(!component.empty())directory.Reset(openat(directory.Get(),component.c_str(),flags));
    if(separator==std::string::npos)break;
    offset=separator+1;
  }
  return directory.Release();
}
bool SameSourceObject(const struct stat& a,const struct stat& b){
  const bool same=a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_mode==b.st_mode&&
    a.st_nlink==b.st_nlink&&a.st_uid==b.st_uid&&a.st_gid==b.st_gid&&a.st_size==b.st_size;
  // atime can change as a consequence of this read; mtime/ctime must not.
#if defined(__APPLE__)
  return same&&a.st_mtimespec.tv_sec==b.st_mtimespec.tv_sec&&a.st_mtimespec.tv_nsec==b.st_mtimespec.tv_nsec&&
    a.st_ctimespec.tv_sec==b.st_ctimespec.tv_sec&&a.st_ctimespec.tv_nsec==b.st_ctimespec.tv_nsec;
#else
  return same&&a.st_mtim.tv_sec==b.st_mtim.tv_sec&&a.st_mtim.tv_nsec==b.st_mtim.tv_nsec&&
    a.st_ctim.tv_sec==b.st_ctim.tv_sec&&a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;
#endif
}
std::string ReadExport(const std::string& path){
  struct stat st{};if(lstat(path.c_str(),&st)!=0||!S_ISREG(st.st_mode)||st.st_size<=0||static_cast<uint64_t>(st.st_size)>MaximumDocumentBytes)
    throw std::runtime_error("Original saver returned an empty, non-ordinary or over-8-MiB document");
  std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Cannot read original saver bytes");
  std::string bytes(static_cast<size_t>(st.st_size),'\0');input.read(bytes.data(),bytes.size());
  if(!input||input.peek()!=std::char_traits<char>::eof()||bytes.find('\0')!=std::string::npos)throw std::runtime_error("Original saved text readback differs in size or contains NUL");
  return bytes;
}
void CloseCompatibility(OriginalDocumentState& state){
  if(!state.compatibilityReady)return;
  close_gl4es();set_getprocaddress(nullptr);set_getmainfbsize(nullptr);
  state.compatibilityReady=false;state.contextGeneration=0;
}
// Refit ONLY for a committed Open or an explicit native ResetView. Ordinary
// Render/Capture/Restore/selection/export and viewport resize retain the same
// reference basis; never infer a reset from matching default camera numbers.
void CentreDocumentView(TGXApp& app,OriginalDocumentState& state,int width,int height,const char* action){
  auto& renderer=app.GetRenderer();const auto previousBasis=renderer.GetBasis();
  try {
    if(renderer.IsPerspectiveEnabled()||renderer.GetStereoFlag()!=0)
      throw std::runtime_error("Original framing requires the ordinary orthographic view");
    renderer.GetBasis().Reset();app.CenterView(true);
    auto defaultBasis=renderer.GetBasis();const Olex2DocumentRequest defaults;
    defaultBasis.RotateX(defaults.pitch);defaultBasis.RotateY(defaults.yaw);
    framing::Point centre{};framing::Matrix rotation{};
    for(unsigned i=0;i<3;++i){
      centre[i]=defaultBasis.GetCenter()[i];
      for(unsigned j=0;j<3;++j)rotation[i][j]=defaultBasis.GetMatrix()[i][j];
    }
    framing::Bounds bounds;size_t worldBoxCount=0;
    for(size_t i=0;i<renderer.ObjectCount();++i){
      auto& object=renderer.GetObject(i);
      if(!object.IsVisible()||!object.HasPrimitives())continue;
      // Identity-drawn overlays are screen-space, not world framing bounds.
      const auto& primitives=object.GetPrimitives();bool worldDraw=false;
      for(size_t j=0;j<primitives.PrimitiveCount();++j)
        if(!primitives.GetPrimitive(j).GetProperties().IsIdentityDraw()){worldDraw=true;break;}
      if(!worldDraw)continue;
      vec3d high,low;if(!object.GetDimensions(high,low))continue;
      bounds.AddBox({low[0],low[1],low[2]},{high[0],high[1],high[2]},centre,rotation);++worldBoxCount;
    }
    // Original bond primitive creation can leave a pending GL error under the
    // gl4es compatibility layer. Clear that pending state BEFORE the framing
    // readback and log it, instead of misattributing it to the projection; the
    // rendered scene is still required to draw and is verified separately.
    {const auto pending=glGetError();if(pending)TBasicApp::NewLogEntry(logWarning)
      << "Pending OpenGL error " << pending << " cleared before original framing (bond primitive creation)";}
    renderer.SetView(false); // refresh the ORIGINAL projection after object creation
    CheckGL("original framing SetView");
    framing::Projection projection{};
    glGetFloatv(GL_PROJECTION_MATRIX,projection.data());CheckGL("original framing projection readback");
    // Project the original centre to obtain its actual eye-space depth. This
    // uses the original renderer, not guessed near/far or a fabricated camera.
    const double eyeOriginZ=renderer.Project(-renderer.GetBasis().GetCenter())[2];
    const double zoom=framing::FitZoom(bounds,width,height,projection,eyeOriginZ,renderer.GetBasis().GetZoom());
    OriginalDocumentFraming observed;observed.applied=true;observed.width=width;observed.height=height;
    observed.worldBoxCount=worldBoxCount;observed.revision=state.revision;observed.contextGeneration=state.contextGeneration;observed.action=action;
    observed.yaw=defaults.yaw;observed.pitch=defaults.pitch;observed.defaultDistance=defaults.distance;
    observed.originalZoom=renderer.GetBasis().GetZoom();observed.zoom=zoom;observed.eyeOriginZ=eyeOriginZ;
    observed.bounds=bounds;observed.centre=centre;observed.rotation=rotation;observed.projection=projection;
    state.lastFraming=std::move(observed);
    renderer.GetBasis().SetZoom(zoom);state.centredBasis=renderer.GetBasis();
  }catch(...){renderer.SetBasis(previousBasis);throw;}
}
void ApplyCamera(TGXApp& app,const OriginalDocumentState& state,const Olex2DocumentRequest& request){
  if(!std::isfinite(request.yaw)||!std::isfinite(request.pitch)||!std::isfinite(request.distance)||request.distance<0.1f||request.distance>100.f)
    throw std::invalid_argument("Invalid original document view parameters");
  auto& basis=app.GetRenderer().GetBasis();basis=state.centredBasis;
  // TEBasis stores absolute angles in degrees, not radians.
  basis.RotateX(request.pitch);basis.RotateY(request.yaw);
  basis.SetZoom(state.centredBasis.GetZoom()*4.4/request.distance);
}
// Use the original renderer's color-ID picking path, not a replacement CPU
// distance test. GLES does not provide legacy GL_SELECT. Restore the setting
// on every exit; the normal original Draw below replaces the ID-color pass.
class OriginalPickMode {
  TGlRenderer& renderer_;bool old_;
public:
  explicit OriginalPickMode(TGlRenderer& renderer):renderer_(renderer),old_(renderer.IsGLUSelection()) {renderer_.SetGLUSelection(false);}
  ~OriginalPickMode(){renderer_.SetGLUSelection(old_);}
};
bool IsDocumentInteractionOperation(Olex2DocumentOperation op){
  return op==Olex2DocumentOperation::Pick||op==Olex2DocumentOperation::ClearSelection||
    op==Olex2DocumentOperation::SelectAll||op==Olex2DocumentOperation::InvertSelection||
    op==Olex2DocumentOperation::ToggleCellVisibility||op==Olex2DocumentOperation::ToggleBasisVisibility||
    op==Olex2DocumentOperation::ToggleFrameVisibility||op==Olex2DocumentOperation::ToggleGridVisibility||
    op==Olex2DocumentOperation::ShowAllVisible||op==Olex2DocumentOperation::CenterModel||
    op==Olex2DocumentOperation::HideSelection||op==Olex2DocumentOperation::ViewAlongSelection||
    op==Olex2DocumentOperation::ViewAlong100||op==Olex2DocumentOperation::ViewAlong010||
    op==Olex2DocumentOperation::ViewAlong001||op==Olex2DocumentOperation::ViewAlong110||
    op==Olex2DocumentOperation::ViewAlong101||op==Olex2DocumentOperation::ViewAlong011||
    op==Olex2DocumentOperation::ViewAlong111||op==Olex2DocumentOperation::SetDrawingQualityLow||
    op==Olex2DocumentOperation::SetDrawingQualityMedium||op==Olex2DocumentOperation::SetDrawingQualityHigh||
    op==Olex2DocumentOperation::ToggleHydrogensVisibility||op==Olex2DocumentOperation::ToggleHydrogenBondsVisibility||
    op==Olex2DocumentOperation::ToggleQPeaksVisibility||op==Olex2DocumentOperation::ToggleQPeakBondsVisibility||
    op==Olex2DocumentOperation::ToggleLabelsVisibility||op==Olex2DocumentOperation::ToggleStructureVisibility||
    op==Olex2DocumentOperation::FragmentSelectAll||op==Olex2DocumentOperation::FragmentSelectAtoms||
    op==Olex2DocumentOperation::FragmentSelectBonds||op==Olex2DocumentOperation::FragmentHide||
    op==Olex2DocumentOperation::FragmentShowOnly;
}
void ResetSelectionObservation(OriginalDocumentState& state){
  state.selectionSequence=0;state.originalPickingCalls=0;state.pickHit=false;
  state.lastPickPixelX=state.lastPickPixelY=-1;state.selectionAction.clear();state.lastPickedCollection.clear();
}
void ApplySelection(TGXApp& app,OriginalDocumentState& state,const Olex2DocumentRequest& request,int width,int height){
  if(!IsDocumentInteractionOperation(request.operation))return;
  TGlRenderer& renderer=app.GetRenderer();
  if(request.selection_mode>2||!std::isfinite(request.pointer_x)||!std::isfinite(request.pointer_y)||
      request.pointer_x<0||request.pointer_x>1||request.pointer_y<0||request.pointer_y>1)
    throw std::invalid_argument("Invalid original selection mode or normalized viewport coordinate");
  // Capture action results independently of document-data revision. Selection
  // does not rewrite a file, invoke a macro or modify the original data model.
  bool hit=false;std::string collection,action;
  if(request.operation==Olex2DocumentOperation::Pick){
    if(width<4||height<4||renderer.ObjectCount()>=0xffffffu)
      throw std::runtime_error("Original color-ID picker cannot represent this viewport or object count");
    const int x=std::max(1,std::min(width-2,static_cast<int>(std::floor(request.pointer_x*width))));
    const int y=std::max(1,std::min(height-2,static_cast<int>(std::floor(request.pointer_y*height))));
    AGDrawObject* object=nullptr;
    {
      OriginalPickMode mode(renderer);
      ++state.originalPickingCalls;state.lastPickPixelX=x;state.lastPickPixelY=y;
      object=renderer.SelectObject(x,y,true);CheckGL("original renderer object picking");
    }
    if(object==&renderer.GetSelection())throw std::runtime_error("Original picker returned its selection container; selection was not changed");
    // The upstream pick can include drawing-only objects. Do not make an
    // unselectable or hidden object selectable just to obtain a passing hit.
    if(object&&(!object->IsVisible()||!object->IsSelectable()))object=nullptr;
    if(object){
      const auto& name=object->GetCollectionName();
      collection=Utf8(name.Length()>128?name.SubString(0,128):name);hit=true;
    }
    if(request.selection_mode==0)renderer.SelectAll(false);
    if(object){
      if(request.selection_mode==2)renderer.Select(*object,!object->IsSelected());
      else renderer.Select(*object,true);
    }
    action="pick";
  }else if(request.operation==Olex2DocumentOperation::ClearSelection){
    renderer.SelectAll(false);action="clear-selection";
  }else if(request.operation==Olex2DocumentOperation::SelectAll){
    renderer.SelectAll(true);action="select-all";
  }else if(request.operation==Olex2DocumentOperation::InvertSelection){
    renderer.InvertSelection();action="invert-selection";
  }else if(request.operation==Olex2DocumentOperation::ToggleCellVisibility){
    if(!DispatchHarmonyMainGuiAction(app,"toggle-cell"))throw std::runtime_error("Harmony main-form cell action is unavailable");
    action="toggle-cell";
  }else if(request.operation==Olex2DocumentOperation::ToggleBasisVisibility){
    if(!DispatchHarmonyMainGuiAction(app,"toggle-basis"))throw std::runtime_error("Harmony main-form basis action is unavailable");
    action="toggle-basis";
  }else if(request.operation==Olex2DocumentOperation::ToggleFrameVisibility){
    if(!DispatchHarmonyMainGuiAction(app,"toggle-frame"))throw std::runtime_error("Harmony main-form frame action is unavailable");
    action="toggle-frame";
  }else if(request.operation==Olex2DocumentOperation::ToggleGridVisibility){
    if(!DispatchHarmonyMainGuiAction(app,"toggle-grid"))
      throw std::runtime_error("No crystallographic grid is loaded; load a map before showing the grid");
    action="toggle-grid";
  }else if(request.operation==Olex2DocumentOperation::ShowAllVisible){
    if(!DispatchHarmonyMainGuiAction(app,"show-all"))throw std::runtime_error("Harmony main-form show-all action is unavailable");
    action="show-all";
  }else if(request.operation==Olex2DocumentOperation::HideSelection){
    if(!DispatchHarmonyMainGuiAction(app,"hide-selection"))
      throw std::runtime_error("Select one or more objects before hiding the selection");
    action="hide-selection";
  }else if(request.operation==Olex2DocumentOperation::SetDrawingQualityLow||
      request.operation==Olex2DocumentOperation::SetDrawingQualityMedium||
      request.operation==Olex2DocumentOperation::SetDrawingQualityHigh){
    action=request.operation==Olex2DocumentOperation::SetDrawingQualityLow?"quality-1":
      request.operation==Olex2DocumentOperation::SetDrawingQualityMedium?"quality-2":"quality-3";
    if(!DispatchHarmonyMainGuiAction(app,action))
      throw std::runtime_error("Harmony main-form drawing quality action is unavailable");
  }else if(request.operation==Olex2DocumentOperation::ToggleHydrogensVisibility||
      request.operation==Olex2DocumentOperation::ToggleHydrogenBondsVisibility||
      request.operation==Olex2DocumentOperation::ToggleQPeaksVisibility||
      request.operation==Olex2DocumentOperation::ToggleQPeakBondsVisibility||
      request.operation==Olex2DocumentOperation::ToggleLabelsVisibility||
      request.operation==Olex2DocumentOperation::ToggleStructureVisibility){
    action=request.operation==Olex2DocumentOperation::ToggleHydrogensVisibility?"toggle-hydrogens":
      request.operation==Olex2DocumentOperation::ToggleHydrogenBondsVisibility?"toggle-hydrogen-bonds":
      request.operation==Olex2DocumentOperation::ToggleQPeaksVisibility?"toggle-qpeaks":
      request.operation==Olex2DocumentOperation::ToggleQPeakBondsVisibility?"toggle-qpeak-bonds":
      request.operation==Olex2DocumentOperation::ToggleLabelsVisibility?"toggle-labels":"toggle-structure";
    if(!DispatchHarmonyMainGuiAction(app,action))
      throw std::runtime_error("Original visibility action is unavailable");
    if(request.operation==Olex2DocumentOperation::ToggleStructureVisibility)
      state.centredBasis.SetCenter(renderer.GetBasis().GetCenter());
  }else if(request.operation==Olex2DocumentOperation::FragmentSelectAll||
      request.operation==Olex2DocumentOperation::FragmentSelectAtoms||
      request.operation==Olex2DocumentOperation::FragmentSelectBonds||
      request.operation==Olex2DocumentOperation::FragmentHide||
      request.operation==Olex2DocumentOperation::FragmentShowOnly){
    action=request.operation==Olex2DocumentOperation::FragmentSelectAll?"fragment-select":
      request.operation==Olex2DocumentOperation::FragmentSelectAtoms?"fragment-select-atoms":
      request.operation==Olex2DocumentOperation::FragmentSelectBonds?"fragment-select-bonds":
      request.operation==Olex2DocumentOperation::FragmentHide?"fragment-hide":"fragment-show-only";
    if(!DispatchHarmonyMainGuiAction(app,action))
      throw std::runtime_error("Select an atom or bond before using an original fragment action");
    if(request.operation==Olex2DocumentOperation::FragmentShowOnly)
      state.centredBasis.SetCenter(renderer.GetBasis().GetCenter());
    if(request.operation==Olex2DocumentOperation::FragmentHide||
        request.operation==Olex2DocumentOperation::FragmentShowOnly){
      // Hidden members cannot belong to a restorable viewport selection; the
      // original fragment actions keep them selected, which the capture schema
      // rejects. Detach only the newly hidden members.
      for(size_t i=0;i<renderer.ObjectCount();++i){
        AGDrawObject& object=renderer.GetObject(i);
        if(&object!=&renderer.GetSelection()&&object.IsSelected()&&!object.IsVisible())
          renderer.Select(object,false);
      }
    }
  }else if(request.operation==Olex2DocumentOperation::CenterModel){
    if(!DispatchHarmonyMainGuiAction(app,"center-model"))throw std::runtime_error("Harmony main-form center action is unavailable");
    // The original action changes only the camera centre. Keep the neutral
    // reference centre in sync so subsequent camera snapshots remain
    // representable without baking the current yaw/pitch into that reference.
    state.centredBasis.SetCenter(app.GetRenderer().GetBasis().GetCenter());
    action="center-model";
  }else if(request.operation==Olex2DocumentOperation::ViewAlong100||
      request.operation==Olex2DocumentOperation::ViewAlong010||request.operation==Olex2DocumentOperation::ViewAlong001||
      request.operation==Olex2DocumentOperation::ViewAlong110||request.operation==Olex2DocumentOperation::ViewAlong101||
      request.operation==Olex2DocumentOperation::ViewAlong011||request.operation==Olex2DocumentOperation::ViewAlong111||
      request.operation==Olex2DocumentOperation::ViewAlongSelection){
    const char* direction=request.operation==Olex2DocumentOperation::ViewAlong100?"100":
      request.operation==Olex2DocumentOperation::ViewAlong010?"010":
      request.operation==Olex2DocumentOperation::ViewAlong001?"001":
      request.operation==Olex2DocumentOperation::ViewAlong110?"110":
      request.operation==Olex2DocumentOperation::ViewAlong101?"101":
      request.operation==Olex2DocumentOperation::ViewAlong011?"011":"111";
    action=std::string("view-along-")+direction;
    if(request.operation==Olex2DocumentOperation::ViewAlongSelection) action="view-along-selection";
    if(!DispatchHarmonyMainGuiAction(app,action))
      throw std::runtime_error(request.operation==Olex2DocumentOperation::ViewAlongSelection ?
        "Select exactly one bond or plane before using view-along-selection" :
        "Harmony main-form crystallographic view action is unavailable");
    // The original `matr` command replaces the orientation matrix directly.
    // Make that orientation the camera reference, with zero relative angles,
    // so capture/restore and subsequent mouse rotation remain representable.
    auto& basis=renderer.GetBasis();basis.ResetAngles();state.centredBasis=basis;
  }
  // The original renderer can leave objects inside its selection container while
  // their selected flag is stale (observed after rollback/restore paths). The
  // viewport inventory requires membership and flag to agree, so reconcile the
  // renderer's own selection state after every successful selection mutation.
  for(size_t i=0;i<renderer.ObjectCount();++i){
    AGDrawObject& object=renderer.GetObject(i);
    if(&object==&renderer.GetSelection())continue;
    const bool member=object.GetParentGroup()==&renderer.GetSelection();
    if(member!=object.IsSelected())object.SetSelected(member);
  }
  ++state.selectionSequence;state.pickHit=hit;state.lastPickedCollection=std::move(collection);state.selectionAction=std::move(action);
}
void ObserveVisibleContent(OriginalDocumentState& state,int width,int height){
  GLfloat clear[4]={};GLint alignment=4;
  glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);
  std::array<std::uint8_t,3> background{};
  for(unsigned i=0;i<3;++i)background[i]=static_cast<std::uint8_t>(std::lround(std::max(0.f,std::min(1.f,clear[i]))*255));
  visibility::Observation observed;
  glPixelStorei(GL_PACK_ALIGNMENT,1);
  try {
    observed=visibility::Observe(width,height,background,[](int x,int y,int w,int h,std::uint8_t* pixels){
      glReadPixels(x,y,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
      CheckGL("original complete-viewport tile readback");
    });
  }catch(...){glPixelStorei(GL_PACK_ALIGNMENT,alignment);throw;}
  glPixelStorei(GL_PACK_ALIGNMENT,alignment);CheckGL("original complete-viewport observation");
  state.sampleWidth=observed.width;state.sampleHeight=observed.height;
  state.nonBackgroundPixels=observed.nonBackgroundPixels;state.sampleReadPixels=observed.readPixels;state.sampleTileCount=observed.tileCount;
  state.centralSampleWidth=observed.centralWidth;state.centralSampleHeight=observed.centralHeight;
  state.centralSampleNonBackgroundPixels=observed.centralNonBackgroundPixels;
  state.contentObserved=state.nonBackgroundPixels>0;
}
std::string FramingJson(const OriginalDocumentFraming& fit){
  if(!fit.applied)return "null";
  std::ostringstream out;out<<std::setprecision(std::numeric_limits<double>::max_digits10);
  auto array=[&](const auto& values){out<<'[';bool first=true;for(const auto& value:values){if(!first)out<<',';first=false;out<<value;}out<<']';};
  out<<"{\"scope\":\"last-successful-native-fit; not current-camera or GPU proof\",\"action\":"<<Quote(fit.action)
    <<",\"width\":"<<fit.width<<",\"height\":"<<fit.height<<",\"revision\":"<<fit.revision<<",\"contextGeneration\":"<<fit.contextGeneration
    <<",\"worldBoxCount\":"<<fit.worldBoxCount<<",\"defaultYaw\":"<<fit.yaw<<",\"defaultPitch\":"<<fit.pitch<<",\"defaultDistance\":"<<fit.defaultDistance
    <<",\"originalZoom\":"<<fit.originalZoom<<",\"referenceZoom\":"<<fit.zoom<<",\"eyeOriginZ\":"<<fit.eyeOriginZ
    <<",\"xyEdgeFraction\":0.05,\"depthEdgeFraction\":0.01,\"centre\":";array(fit.centre);
  out<<",\"viewBoundsMin\":";array(fit.bounds.minimum);out<<",\"viewBoundsMax\":";array(fit.bounds.maximum);
  out<<",\"defaultRotationRows\":[";for(unsigned i=0;i<3;++i){if(i)out<<',';array(fit.rotation[i]);}out<<']';
  out<<",\"projectionColumnMajor\":";array(fit.projection);out<<'}';return out.str();
}
// The existing application bridge accepts flat scalar status fields ONLY.
// Keep the complete parameter object as a quoted JSON string; expose binding
// dimensions/IDs as scalars so no app/parser or camera-schema change is needed.
std::string FramingStatusFields(const OriginalDocumentFraming& fit){
  std::ostringstream out;out<<",\"framingApplied\":"<<(fit.applied?"true":"false")
    <<",\"framingWidth\":"<<fit.width<<",\"framingHeight\":"<<fit.height
    <<",\"framingRevision\":"<<fit.revision<<",\"framingContextGeneration\":"<<fit.contextGeneration
    <<",\"framingWorldBoxCount\":"<<fit.worldBoxCount<<",\"framing\":"<<Quote(FramingJson(fit));
  return out.str();
}
std::string Status(const OriginalDocumentState& state,TGXApp& app,bool passed,const std::string& error,bool changed){
  size_t visible=0;if(state.loaded){auto atoms=app.GetAtoms();while(atoms.HasNext())if(atoms.Next().IsVisible())++visible;}
  const auto& renderer=app.GetRenderer();
  std::ostringstream view;view<<renderer.GetBasis().GetCenter()[0]<<','<<renderer.GetBasis().GetCenter()[1]<<','<<renderer.GetBasis().GetCenter()[2];
  const char* interactionSource="original TGlRenderer::SelectObject and original TGlGroup";
  if(state.selectionAction=="toggle-cell")interactionSource="original TMainForm::OnCellVisible/TGXApp::SetCellVisible";
  else if(state.selectionAction=="toggle-basis")interactionSource="original TMainForm::OnBasisVisible/TGXApp::SetBasisVisible";
  else if(state.selectionAction=="toggle-frame")interactionSource="original TMainForm::FrameVChange/T3DFrameCtrl::SetVisible";
  else if(state.selectionAction=="toggle-grid")interactionSource="original TMainForm::GridVChange/TGXApp::ShowGrid";
  else if(state.selectionAction=="show-all")interactionSource="original TMainForm::OnShowAll/GXLibMacros::macFmol";
  else if(state.selectionAction=="center-model")interactionSource="original TMainForm::OnModelCenter/GXLibMacros::macCenter";
  else if(state.selectionAction=="hide-selection")interactionSource="original TMainForm::OnGraphics/GXLibMacros::macHide";
  else if(state.selectionAction=="toggle-labels")interactionSource="original TGXApp::AreLabelsVisible/TGXApp::SetLabelsVisible/stateLabelsVisible";
  else if(state.selectionAction=="toggle-structure")interactionSource="original TMainForm::macShowStr/TGXApp::SetStructureVisible";
  else if(state.selectionAction=="quality-1"||state.selectionAction=="quality-2"||state.selectionAction=="quality-3")
    interactionSource="original TMainForm::OnDrawQChange/GXLibMacros::macQual/TGXApp::Quality";
  else if(state.selectionAction=="toggle-hydrogens"||state.selectionAction=="toggle-hydrogen-bonds")
    interactionSource="original GXLibMacros::macShowH/TGXApp hydrogen visibility setters";
  else if(state.selectionAction=="toggle-qpeaks"||state.selectionAction=="toggle-qpeak-bonds")
    interactionSource="original GXLibMacros::macShowQ/TGXApp Q-peak visibility setters";
  else if(state.selectionAction=="fragment-hide"||state.selectionAction=="fragment-show-only"||
      state.selectionAction=="fragment-select"||state.selectionAction=="fragment-select-atoms"||
      state.selectionAction=="fragment-select-bonds")
    interactionSource="original TMainForm fragment actions/TGXApp::FragmentsVisible and SelectFragments";
  else if(state.selectionAction.rfind("view-along-",0)==0)interactionSource=state.selectionAction=="view-along-selection"?
    "original TMainForm::OnBond/OnPlane/GXLibMacros::macSetView":"original TMainForm::OnViewAlong/GXLibMacros::macMatr";
  std::ostringstream out;out<<"{\"passed\":"<<(passed?"true":"false")<<",\"loaded\":"<<(state.loaded?"true":"false")
    <<",\"name\":"<<Quote(state.name)<<",\"format\":"<<Quote(state.format)<<",\"byteCount\":"<<state.byteCount<<",\"sha256\":"<<Quote(state.sha256)
    <<",\"revision\":"<<state.revision<<",\"frames\":"<<state.frames<<",\"atomCount\":"<<(state.loaded?app.GetAtoms().count:0)
    <<",\"bondCount\":"<<(state.loaded?app.GetBonds().count:0)<<",\"exportByteCount\":"<<state.exportByteCount<<",\"error\":"<<Quote(error)
    <<",\"modelMayHaveChanged\":"<<(changed?"true":"false")<<",\"contextRetained\":"<<(state.compatibilityReady?"true":"false")
    <<",\"visibleSites\":"<<visible<<",\"renderObjects\":"<<renderer.ObjectCount()<<",\"renderCollections\":"<<renderer.CollectionCount()
    <<",\"renderPrimitives\":"<<renderer.PrimitiveCount()<<",\"sceneComplete\":"<<(renderer.IsSceneComplete()?"true":"false")
    <<",\"viewZoom\":"<<Quote(std::to_string(renderer.GetBasis().GetZoom()))<<",\"viewCenter\":"<<Quote(view.str())
    <<",\"sampleWidth\":"<<state.sampleWidth<<",\"sampleHeight\":"<<state.sampleHeight<<",\"sampleNonBackgroundPixels\":"<<state.nonBackgroundPixels
    <<",\"sampleStrategy\":\"bounded-complete-framebuffer-tiles\",\"sampleReadPixels\":"<<state.sampleReadPixels<<",\"sampleTileCount\":"<<state.sampleTileCount
    <<",\"centralSampleWidth\":"<<state.centralSampleWidth<<",\"centralSampleHeight\":"<<state.centralSampleHeight<<",\"centralSampleNonBackgroundPixels\":"<<state.centralSampleNonBackgroundPixels
    <<",\"selectedObjectCount\":"<<(state.loaded?renderer.GetSelection().Count():0)<<",\"selectedParts\":"<<Quote(SelectedParts(app))<<",\"selectionSequence\":"<<state.selectionSequence
    <<",\"pickHit\":"<<(state.pickHit?"true":"false")<<",\"lastPickedCollection\":"<<Quote(state.lastPickedCollection)
    <<",\"selectionAction\":"<<Quote(state.selectionAction)<<",\"viewHash\":"<<Quote(ViewHash(app));
  if(state.loaded)out<<",\"bondRadius\":"<<TXBond::GetSettings(app.GetRenderer()).GetRadius()
    <<",\"labelsVisible\":"<<(app.AreLabelsVisible()?"true":"false")
    <<",\"hydrogensVisible\":"<<(app.AreHydrogensVisible()?"true":"false")
    <<",\"hydrogenBondsVisible\":"<<(app.AreHBondsVisible()?"true":"false")
    <<",\"qPeaksVisible\":"<<(app.AreQPeaksVisible()?"true":"false")
    <<",\"qPeakBondsVisible\":"<<(app.AreQPeakBondsVisible()?"true":"false")
    <<",\"structureVisible\":"<<(app.IsStructureVisible()?"true":"false")
    <<",\"cellVisible\":"<<(app.IsCellVisible()?"true":"false")
    <<",\"basisVisible\":"<<(app.IsBasisVisible()?"true":"false")
    <<",\"frameVisible\":"<<(app.Get3DFrame().IsVisible()?"true":"false")
    <<",\"gridVisible\":"<<(app.IsGridVisible()?"true":"false");
  out<<",\"originalPickingCalls\":"<<state.originalPickingCalls
    <<",\"lastPickPixelX\":"<<state.lastPickPixelX<<",\"lastPickPixelY\":"<<state.lastPickPixelY
    <<",\"selectionSource\":"<<Quote(interactionSource)
    <<FramingStatusFields(state.lastFraming)
    <<",\"source\":\"original TGXApp/TGXFile + original TGlRenderer\"}";return out.str();
}
}
std::string ReadOriginalDocumentSourceLocked(const TGXApp& app,const OriginalDocumentState& state){
  if(!state.loaded||app.XFiles().Count()!=1||!app.XFile().HasLastLoader())
    throw std::runtime_error("No original document is loaded");
  if(state.byteCount==0||state.byteCount>MaximumDocumentBytes||state.sha256.size()!=64||
      state.directory.empty()||Format(state.name)!=state.format)
    throw std::runtime_error("Original document source metadata is invalid");
  // The loader's filename may be changed by an earlier canonical export. Only
  // the source identity retained on successful load names the original input.
  SourceDescriptor directory(OpenSourceDirectory(state.directory));
  struct stat directoryBefore{},namedBefore{};
  if(fstat(directory.Get(),&directoryBefore)!=0||!S_ISDIR(directoryBefore.st_mode)||
      directoryBefore.st_uid!=geteuid()||(directoryBefore.st_mode&0077)!=0)
    throw std::runtime_error("Original document stage is not a private owned directory");
  if(fstatat(directory.Get(),state.name.c_str(),&namedBefore,AT_SYMLINK_NOFOLLOW)!=0||
      !S_ISREG(namedBefore.st_mode)||namedBefore.st_nlink!=1||namedBefore.st_uid!=geteuid()||
      (namedBefore.st_mode&0077)!=0||namedBefore.st_size<=0||
      static_cast<uint64_t>(namedBefore.st_size)>MaximumDocumentBytes||
      static_cast<uint64_t>(namedBefore.st_size)!=state.byteCount)
    throw std::runtime_error("Original document source is missing, non-private, non-ordinary or has an invalid size");
  // O_NONBLOCK prevents a replacement FIFO from blocking before fstat rejects
  // it. O_NOFOLLOW rejects a replacement symlink without opening its target.
  SourceDescriptor source(openat(directory.Get(),state.name.c_str(),O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC));
  struct stat opened{};
  if(fstat(source.Get(),&opened)!=0||!SameSourceObject(namedBefore,opened))
    throw std::runtime_error("Original document source changed while opening");
  std::string bytes(state.byteCount,'\0');
  size_t offset=0;
  while(offset<bytes.size()){
    const auto count=read(source.Get(),bytes.data()+offset,bytes.size()-offset);
    if(count<0&&errno==EINTR)continue;
    if(count<=0)throw std::runtime_error("Cannot read the complete original document source");
    offset+=static_cast<size_t>(count);
  }
  char extra=0;ssize_t tail;
  do {tail=read(source.Get(),&extra,1);}while(tail<0&&errno==EINTR);
  if(tail!=0||bytes.find('\0')!=std::string::npos||Hash(bytes)!=state.sha256)
    throw std::runtime_error("Original document source bytes differ from the loaded document");
  // Reopen the recorded absolute directory without following links, then
  // compare both the descriptor and current directory entry after hashing.
  SourceDescriptor currentDirectory(OpenSourceDirectory(state.directory));
  struct stat after{},namedAfter{},directoryAfter{};
  if(fstat(currentDirectory.Get(),&directoryAfter)!=0||!SameSourceObject(directoryBefore,directoryAfter)||
      fstatat(currentDirectory.Get(),state.name.c_str(),&namedAfter,AT_SYMLINK_NOFOLLOW)!=0||
      !SameSourceObject(opened,namedAfter)||fstat(source.Get(),&after)!=0||!SameSourceObject(opened,after))
    throw std::runtime_error("Original document source changed during reading");
  return bytes;
}
std::string RunOriginalDocumentLocked(const Olex2LegacyHost& host,TGXApp& app,PlatformWindowScene& scene,
  PlatformSceneLifetime& lifetime,OriginalDocumentState& state){
  if(host.completed)*host.completed=0;
  bool changed=false,passed=false;std::string error,rollbackError,viewportOutput;uint64_t startDraw=lifetime.endDraw;
  const bool viewportRequest=host.document_request&&(host.document_request->operation==Olex2DocumentOperation::CaptureViewport||host.document_request->operation==Olex2DocumentOperation::RestoreViewport);
  std::optional<OriginalDocumentViewport> targetViewport;
  std::optional<viewport_objects::Inventory> beforeInventory;
  std::optional<OriginalDocumentViewport> beforeViewport;
  TEBasis previousViewportBasis;bool viewportMutated=false;
  auto observeViewport=[&](const viewport_objects::Inventory& inventory){
    // Descriptor-safe byte readback, not a saver/export or caller-supplied hash.
    ReadOriginalDocumentSourceLocked(app,state);
    OriginalDocumentViewport snapshot;snapshot.name=state.name;snapshot.byteCount=state.byteCount;snapshot.sha256=state.sha256;
    snapshot.objectCount=inventory.objects.size();snapshot.inventorySha256=inventory.sha256;snapshot.selectedObjectIndices=inventory.selected;
    viewport_objects::ReadCamera(snapshot,app.GetRenderer(),state.centredBasis);
    ValidateOriginalDocumentViewport(snapshot);return snapshot;
  };
  try {
    if(!host.document_request)throw std::invalid_argument("Missing original document request");
    const auto& request=*host.document_request;
    if(viewportRequest){
      if(!request.viewport_result)throw std::invalid_argument("Missing original viewport output destination");
      if(!state.loaded||!state.compatibilityReady||!state.frames||!state.contentObserved)
        throw std::runtime_error("No successfully loaded/drawn original viewport is available");
      if(request.operation==Olex2DocumentOperation::RestoreViewport)targetViewport=ParseOriginalDocumentViewport(request.viewport_snapshot);
    }
    if(!host.resolve||!host.framebuffer_size||!host.make_current||!host.files_dir||!host.make_current())throw std::runtime_error("No current original document surface");
    if(&TGXApp::GetInstance()!=&app||&app.GetRenderer().GetScene()!=&scene||scene.IsBound()||scene.IsEnabled()||(lifetime.cleanupFailed&&request.operation!=Olex2DocumentOperation::Close))
      throw std::runtime_error("Original document owner/scene is not detached and healthy");
    if(!request.context_generation)throw std::invalid_argument("Missing retained original context generation");
    if(state.compatibilityReady&&request.context_generation!=state.contextGeneration)
      throw std::runtime_error("The original document context changed without a successful Close; document retained, automatic replacement is not supported yet");
    if(request.operation==Olex2DocumentOperation::Open){
      Format(request.name);
      if(request.bytes.empty()||request.bytes.size()>MaximumDocumentBytes||request.bytes.find('\0')!=std::string::npos)
        throw std::invalid_argument("Document must contain nonempty text, no NUL, at most 8 MiB");
    }
    int width=0,height=0;host.framebuffer_size(&width,&height);
    if(width<1||height<1||width>8192||height>8192)throw std::runtime_error("Invalid original document surface size");
    if(!state.compatibilityReady){
      set_getprocaddress(host.resolve);set_getmainfbsize(host.framebuffer_size);initialize_gl4es();
      state.compatibilityReady=true;state.contextGeneration=request.context_generation;
    }
    CurrentDirectory cwd;scene.Bind(host);auto& renderer=app.GetRenderer();
    try {
      if(!scene.MakeCurrent())throw std::runtime_error("Cannot lease original document context");
      renderer.Resize(width,height);CheckGL("original document attach");
      if(!scene.FindFont("Default"))scene.CreateFont("Default","ohos-native:18:r");
      if(request.operation==Olex2DocumentOperation::Open){
        if(app.XFiles().Count()!=1)throw std::runtime_error("Original document file list is not in the supported single-document state");
        OwnedStage staged(host.files_dir);staged.Write(request.name,request.bytes);
        const auto previousBasis=renderer.GetBasis();TGXFile* previousFile=&app.XFile();
        std::vector<AGDrawObject*> previousSelection;
        if(state.loaded){
          const auto inventory=viewport_objects::ReadInventory(app);
          for(size_t index:inventory.selected)previousSelection.push_back(inventory.objects[index]);
        }else if(renderer.GetSelection().Count()!=0){
          throw std::runtime_error("Unloaded original source retains selection; explicit Close is required");
        }
        TGXFile* candidate=nullptr;bool committed=false;
        try {
          // Detach live old members BEFORE the loader/activation can release or
          // reset their flags. ResetSelectionObservation resets telemetry only.
          renderer.SelectAll(false);
          if(renderer.GetSelection().Count()!=0)throw std::runtime_error("Original old selection was not detached before source replacement");
          // The old document stays owned until the original candidate loader
          // and original activation have succeeded. No emulated parser/model.
          candidate=&app.NewXFile();candidate->LoadFromFile(Text(staged.Path(request.name)));
          if(!candidate->HasLastLoader()||candidate->GetAsymmUnit().AtomCount()==0)throw std::runtime_error("Original loader returned no displayable sites");
          app.SetActiveXFile(1);
          // No fallible drawing after deleting the old file may leave metadata
          // claiming the previous file. Adopt the new identity before commit.
          app.DeleteXFile(1);committed=true;changed=true;
          state.loaded=true;state.name=request.name;state.format=Format(request.name);state.byteCount=request.bytes.size();
          state.sha256=Hash(request.bytes);state.exportByteCount=0;++state.revision;state.frames=0;state.contentObserved=false;state.sampleWidth=state.sampleHeight=state.nonBackgroundPixels=0;
          state.sampleReadPixels=state.sampleTileCount=state.centralSampleWidth=state.centralSampleHeight=state.centralSampleNonBackgroundPixels=0;
          RemoveOwnedDirectory(state.directory);state.directory.swap(staged.directory);
          ResetSelectionObservation(state);state.lastFraming=OriginalDocumentFraming{};app.CreateObjects(true);
          // A new source needs a reproducible centred basis, independent of the
          // previous document camera. This occurs ONLY after new-source commit.
          CentreDocumentView(app,state,width,height,"open");
        } catch(...) {
          if(!committed&&app.XFiles().Count()==2){
            try {
              // SetActiveXFile can throw after swapping. Inspect identity,
              // not a success flag, to restore the previous original file.
              if(candidate && &app.XFile()==candidate)app.SetActiveXFile(1);
              if(candidate && &app.XFile()==candidate)throw std::runtime_error("Original activation rollback did not restore the previous file");
              // Release candidate selection while both sources are still
              // alive, before its original objects can be deleted.
              renderer.SelectAll(false);app.DeleteXFile(1);renderer.SetBasis(previousBasis);
            }catch(...){changed=true;rollbackError=CurrentExceptionText();}
          }else if(candidate && &app.XFile()==candidate){
            // The original deletion may throw after it removed the previous
            // file. Keep the surviving new file and its staged bytes truthful.
            changed=true;
            if(!committed){
              state.loaded=app.XFile().HasLastLoader();state.name=request.name;state.format=Format(request.name);
              state.byteCount=request.bytes.size();state.sha256=Hash(request.bytes);++state.revision;state.frames=0;
              RemoveOwnedDirectory(state.directory);state.directory.swap(staged.directory);
              state.centredBasis=renderer.GetBasis();
            }
          }
          if(!committed&&&app.XFile()==previousFile){
            try{
              renderer.SelectAll(false);
              for(auto* object:previousSelection)renderer.Select(*object,true);
              if(renderer.GetSelection().Count()!=previousSelection.size())throw std::runtime_error("Original previous selection rollback count differs");
              for(auto* object:previousSelection)if(!object->IsSelected()||!renderer.GetSelection().Contains(*object))throw std::runtime_error("Original previous selection rollback membership differs");
            }catch(...){changed=true;rollbackError=CurrentExceptionText();}
          }
          throw;
        }
      }else if(request.operation==Olex2DocumentOperation::Close){
        // The original selection group owns live object references. Detach them
        // before the source Close releases its objects; never leave stale members.
        renderer.SelectAll(false);
        if(renderer.GetSelection().Count()!=0)throw std::runtime_error("Original selection was not detached before Close");
        changed=state.loaded;app.XFile().Close();renderer.BeforeContextChange();scene.ReleaseFontResources();CheckGL("original document close");
        lifetime.cleanupFailed=false;state.loaded=false;state.name.clear();state.format.clear();state.sha256.clear();state.byteCount=0;state.exportByteCount=0;++state.revision;
        RemoveOwnedDirectory(state.directory);ResetSelectionObservation(state);state.lastFraming=OriginalDocumentFraming{};scene.Unbind();cwd.Restore();CloseCompatibility(state);
        passed=true;if(host.completed)*host.completed=1;return Status(state,app,true,"",false);
      }else if(!state.loaded){throw std::runtime_error("No original document is open");}
      if(request.operation==Olex2DocumentOperation::ResetView){CentreDocumentView(app,state,width,height,"reset");}
      if(request.operation==Olex2DocumentOperation::Export){
        if(!request.exportedBytes)throw std::invalid_argument("Missing original export destination");
        OwnedStage output(host.files_dir);const auto path=output.Path("export."+state.format);
        app.XFile().SaveToFile(Text(path));std::string bytes=ReadExport(path);
        state.exportByteCount=bytes.size();*request.exportedBytes=std::move(bytes);
      }
      if(viewportRequest){
        beforeInventory=viewport_objects::ReadInventory(app);beforeViewport=observeViewport(*beforeInventory);
        previousViewportBasis=renderer.GetBasis();
        if(targetViewport){
          const auto& target=*targetViewport;const auto& current=*beforeViewport;
          if(target.name!=current.name||target.byteCount!=current.byteCount||target.sha256!=current.sha256)
            throw std::invalid_argument("Viewport source name/bytes/SHA256 do not match the current original document");
          if(target.objectCount!=current.objectCount||target.inventorySha256!=current.inventorySha256)
            throw std::invalid_argument("Viewport complete ordered original inventory does not match; selection untouched");
          viewport_objects::ValidateSelection(*beforeInventory,target.selectedObjectIndices,renderer);
          viewport_objects::ValidateSelection(*beforeInventory,current.selectedObjectIndices,renderer);
          if(state.selectionSequence==std::numeric_limits<uint64_t>::max())throw std::runtime_error("Original selection sequence exhausted");
          const auto targetBasis=viewport_objects::CameraBasis(state.centredBasis,target.yaw,target.pitch,target.distance);
          if(!std::isfinite(targetBasis.GetZoom())||targetBasis.GetZoom()<=0)throw std::invalid_argument("Viewport target basis cannot be represented");
          // All JSON, source, complete inventory and all target selectability/
          // draw-mode checks precede the first selection/camera mutation.
          viewportMutated=true;
          viewport_objects::ApplySelection(renderer,*beforeInventory,target.selectedObjectIndices);
          renderer.SetBasis(targetBasis);
        }
        // Capture deliberately does NOT ApplyCamera(request): observe the real
        // basis without replacing it with the caller/controller camera cache.
      }else{ApplyCamera(app,state,request);ApplySelection(app,state,request,width,height);}
      if(request.operation==Olex2DocumentOperation::Open){
        //43 actual window evidence showed the first original scene pass only
        //settled its drawing state: the complete markers appeared on the next
        //normal Draw. Finish that initialization pass in the SAME retained
        //context before presenting, rather than asking the user to refresh or
        //claiming that a few non-background pixels certify a stable first view.
        const auto beforeInitialization=lifetime.endDraw;
        renderer.Draw();glFinish();CheckGL("original document initial scene pass");
        if(lifetime.endDraw==beforeInitialization)throw std::runtime_error("Original initial scene pass did not finish");
      }
      renderer.Draw();glFinish();CheckGL("original document draw");
      if(request.operation==Olex2DocumentOperation::Open||request.operation==Olex2DocumentOperation::ResetView||!state.contentObserved){
        ObserveVisibleContent(state,width,height);
        if(state.nonBackgroundPixels==0)throw std::runtime_error("Document is retained, but the sampled viewport contains only the clear color; visible-content verification failed");
      }
      if(lifetime.endDraw==startDraw)throw std::runtime_error("Original renderer did not finish a scene draw");
      if(viewportRequest){
        const auto inventory=viewport_objects::ReadInventory(app);const auto observed=observeViewport(inventory);
        const auto& expected=targetViewport?*targetViewport:*beforeViewport;
        if(observed.name!=expected.name||observed.byteCount!=expected.byteCount||observed.sha256!=expected.sha256||
           observed.objectCount!=expected.objectCount||observed.inventorySha256!=expected.inventorySha256||
           observed.selectedObjectIndices!=expected.selectedObjectIndices||
           !viewport_objects::SameBasis(renderer.GetBasis(),targetViewport?
             viewport_objects::CameraBasis(state.centredBasis,expected.yaw,expected.pitch,expected.distance):previousViewportBasis))
          throw std::runtime_error("Original viewport post-draw source/inventory/selection/basis readback differs");
        viewportOutput=OriginalDocumentViewportJson(observed);
      }
      state.frames+=lifetime.endDraw-startDraw;scene.Unbind();cwd.Restore();
      if(viewportRequest)*request.viewport_result=std::move(viewportOutput);
      if(targetViewport){++state.selectionSequence;state.selectionAction="restore-selection";state.pickHit=false;
        state.lastPickedCollection.clear();state.lastPickPixelX=state.lastPickPixelY=-1;}
      passed=true;
    } catch(...) {
      const auto failure=std::current_exception();
      if(viewportMutated&&beforeInventory){
        try {
          // Best effort in-memory rollback using the SAME original APIs. A
          // failed draw/swap is never described as a successful visible rollback.
          viewport_objects::ApplySelection(renderer,*beforeInventory,beforeInventory->selected);
          renderer.SetBasis(previousViewportBasis);
          const auto restored=viewport_objects::ReadInventory(app);
          if(restored.sha256!=beforeInventory->sha256||restored.selected!=beforeInventory->selected||
             !viewport_objects::SameBasis(renderer.GetBasis(),previousViewportBasis))
            throw std::runtime_error("Rollback memory readback differs");
          rollbackError="selection/basis restored in memory only; rollback presentation NOT verified";
        }catch(...){rollbackError="viewport rollback incomplete: "+CurrentExceptionText();}
      }
      scene.Unbind();cwd.Restore();std::rethrow_exception(failure);
    }
  } catch(const TExceptionBase& e){error=Utf8(e.GetException()->GetFullMessage());}
    catch(const std::exception& e){error=e.what();}
    catch(...){error="Unknown original document operation error";}
  if(!rollbackError.empty())error+="; rollback: "+rollbackError;
  if(!passed&&!state.loaded&&state.compatibilityReady){
    // A failed first load has no live document to preserve. Retire only this
    // session's GL objects while its context is still current.
    try {if(!scene.IsBound())scene.Bind(host);app.GetRenderer().BeforeContextChange();scene.ReleaseFontResources();scene.Unbind();CloseCompatibility(state);}
    catch(...){const std::string detail=CurrentExceptionText();scene.Unbind();lifetime.cleanupFailed=true;error+="; original session cleanup was incomplete: "+detail;}
  }
  if(host.completed)*host.completed=passed?1:0;
  return Status(state,app,passed,error,changed&&!passed);
}
}
