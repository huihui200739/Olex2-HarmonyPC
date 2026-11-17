// Source-built libpng decoding through the genuine TGXApp virtual texture seam.
// No GL upload, font creation, structure files or research workflow is executed.
#include "png_gx_texture_app.h"
#include "glscene.h"
#include "original_interpreter.h"
#include <dirent.h>
#include <sys/stat.h>
#include <cstdlib>
#include <cerrno>
#include <vector>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
namespace {
std::string output;
struct Counts {unsigned destroyed=0,fontRequests=0,currentAttempts=0;};
class DisabledScene final:public AGlScene {
  Counts& counts;
  TGlFont& DoCreateFont(TGlFont&,bool) const override {++counts.fontRequests;throw std::runtime_error("Font backend not connected");}
public:
  explicit DisabledScene(Counts& c):counts(c){}
  ~DisabledScene() override {++counts.destroyed;}
  void ScaleFonts(double) override {throw std::runtime_error("Font backend not connected");}
  void RestoreFontScale() override {throw std::runtime_error("Font backend not connected");}
  olxstr ShowFontDialog(TGlFont*,const olxstr&) override {throw std::runtime_error("Font dialog not connected");}
  bool MakeCurrent() override {++counts.currentAttempts;return false;}
};
unsigned DescriptorCount() {
  DIR* dir=opendir("/proc/self/fd");if(!dir)throw std::runtime_error("Cannot inspect own descriptor lifecycle");
  unsigned count=0;
  while(auto* entry=readdir(dir))if(entry->d_name[0]!='.')++count;
  closedir(dir);return count;
}
std::vector<std::string> DescriptorIdentities() {
  DIR* dir=opendir("/proc/self/fd");if(!dir)throw std::runtime_error("Cannot inspect own descriptors");
  const int own=dirfd(dir);std::vector<std::string> result;
  while(auto* entry=readdir(dir)) {
    char* end=nullptr;long fd=std::strtol(entry->d_name,&end,10);
    if(!end||*end||end==entry->d_name||fd<0||fd==own)continue;
    struct stat state{};
    std::string row=std::to_string(fd)+":";
    if(fstat(static_cast<int>(fd),&state)==0)row+=std::to_string(state.st_dev)+":"+std::to_string(state.st_ino)+":"+std::to_string(state.st_mode);
    else row+="fstat-error:"+std::to_string(errno);
    result.push_back(row);
  }
  closedir(dir);return result;
}
std::string Quote(const std::string& text) {
  std::ostringstream out;out<<'"';for(unsigned char c:text){
    if(c=='"'||c=='\\')out<<'\\'<<c;else if(c>=32)out<<c;
    else out<<"\\u00"<<"0123456789abcdef"[c>>4]<<"0123456789abcdef"[c&15];
  }out<<'"';return out.str();
}
}
extern "C" const char* olex2_gx_texture_decode_run(const char* base,const char* directory) {
  using namespace olex2::harmony;
  std::ostringstream out;unsigned latestCycle=0,latestBefore=0,latestAfter=0,latestDestroyed=0,latestFonts=0;bool latestReleased=false;
  try {
    OriginalInterpreterGuard guard;
    if(!base||base[0]!='/'||!directory||directory[0]!='/'||TBasicApp::HasInstance())throw std::runtime_error("An exclusive empty platform application is required");
    out<<"{\"scope\":\"TGXApp virtual decoder seam only; exact PNG RGB samples, no GL upload or fonts\",\"backend\":\"libpng-unassociated-rgb\",\"cycles\":[";
    for(unsigned cycle=1;cycle<=3;++cycle) {
      Counts counts;std::unique_ptr<DisabledScene> scene(new DisabledScene(counts));
      std::unique_ptr<PngTextureGXApp> app(new PngTextureGXApp(olxstr::FromUTF8(base),scene.get()));scene.release();
      TGXApp& originalInterface=*app;
      const bool identity=&TGXApp::GetInstance()==app.get();
      latestCycle=cycle;const auto beforeIdentities=DescriptorIdentities();
      const unsigned before=DescriptorCount();latestBefore=before;
      if(cycle>1)out<<',';out<<"{\"cycle\":"<<cycle<<",\"original_graphics_identity\":"<<identity<<",\"fd_before\":"<<before<<",\"images\":[";
      bool first=true;
      for(const char* filename:{"synthetic-rgb-2.png","synthetic-rgba-2.png","synthetic-rgb-1.png","synthetic-rgb-3.png","synthetic-opaque-rgba-4.png","合成颜色-2.png","synthetic-no-image-selected.png"}) {
        const std::string path=std::string(directory)+"/"+filename;int size=77;std::vector<unsigned char> rgb={1,2,3};
        const bool accepted=originalInterface.ReadPlatformTexture(olxstr::FromUTF8(path.c_str()),size,rgb);
        const auto& image=app->LastTextureImage();const auto& png=app->LastPngTexture();
        if(!first)out<<',';first=false;
        out<<"{\"fixture\":"<<Quote(filename)<<",\"accepted\":"<<(accepted?"true":"false")<<",\"status\":"<<Quote(TextureImageStatusName(image.status))
           <<",\"size\":"<<size<<",\"width\":"<<image.width<<",\"height\":"<<image.height<<",\"stride\":"<<image.rowStride
           <<",\"format\":"<<image.pixelFormat<<",\"alpha_metadata\":"<<image.alphaType<<",\"read_bytes\":"<<image.readBytes
           <<",\"error\":"<<Quote(image.error)<<",\"resource_audit\":"<<ImageResourceAuditJson(image.resourceAudit)
           <<",\"png_decoder\":{\"version\":"<<Quote(olex2_png_decoder_version())<<",\"source_bit_depth\":"<<png.sourceBitDepth
           <<",\"source_color_type\":"<<png.sourceColorType<<",\"source_interlace\":"<<png.sourceInterlace
           <<",\"allocations\":"<<png.allocations<<",\"releases\":"<<png.releases<<",\"live_allocations\":"<<png.liveAllocations
           <<",\"live_bytes\":"<<png.liveBytes<<",\"warnings\":"<<png.warnings<<"},\"rgb\":[";
        for(size_t i=0;i<rgb.size();++i){if(i)out<<',';out<<unsigned(rgb[i]);}out<<"]}";
      }
      const unsigned after=DescriptorCount();latestAfter=after;const auto afterIdentities=DescriptorIdentities();
      app.reset();const bool released=!TBasicApp::HasInstance();latestReleased=released;latestDestroyed=counts.destroyed;latestFonts=counts.fontRequests;
      out<<"],\"fd_after\":"<<after<<",\"scene_destroyed\":"<<counts.destroyed<<",\"font_requests\":"<<counts.fontRequests
         <<",\"make_current_attempts\":"<<counts.currentAttempts<<",\"singleton_cleared\":"<<released<<",\"fd_identities_before\":[";
      for(size_t i=0;i<beforeIdentities.size();++i){if(i)out<<',';out<<Quote(beforeIdentities[i]);}out<<"],\"fd_identities_after\":[";
      for(size_t i=0;i<afterIdentities.size();++i){if(i)out<<',';out<<Quote(afterIdentities[i]);}out<<"]}";
      if(!identity||before!=after||!released||counts.destroyed!=1||counts.fontRequests!=0)throw std::runtime_error("PNG decoder application resources did not balance");
    }
    out<<"],\"execution_completed\":true}";output=out.str();
  }catch(const TExceptionBase& e){output="{\"execution_completed\":false,\"error\":"+Quote(e.GetException()->GetFullMessage().ToMBStr().c_str())+"}";
  }catch(const std::exception& e){std::ostringstream error;error<<"{\"execution_completed\":false,\"error\":"<<Quote(e.what())<<",\"last_cycle\":"<<latestCycle<<",\"fd_before\":"<<latestBefore<<",\"fd_after\":"<<latestAfter<<",\"scene_destroyed\":"<<latestDestroyed<<",\"font_requests\":"<<latestFonts<<",\"singleton_cleared\":"<<latestReleased<<",\"partial_trace\":"<<Quote(out.str())<<'}';output=error.str();}
  return output.c_str();
}
