#pragma once
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace olex2::harmony {
struct FontDisplayMetrics {
  bool ready=false;
  uint64_t displayId=0,revision=0;
  double dpiX=0,dpiY=0,densityPixels=0;
};
inline void ValidateFontDisplayObservation(double id,double x,double y,double density) {
  if(!std::isfinite(id)||id<0||id>9007199254740991.0||std::floor(id)!=id)
    throw std::invalid_argument("Display ID must be an observed nonnegative safe integer");
  // Reject, never clamp or silently substitute 96 DPI or densityDPI.
  if(!std::isfinite(x)||!std::isfinite(y)||x<=0||y<=0||x>8192||y>8192)
    throw std::invalid_argument("Physical display DPI must be finite, positive and bounded");
  if(!std::isfinite(density)||density<=0||density>64)
    throw std::invalid_argument("Observed display densityPixels is invalid");
  const double ratio=x/y;
  if(ratio<0.0625||ratio>16)
    throw std::invalid_argument("Physical display pixel aspect exceeds supported raster bounds");
}
inline void RequireFontDisplayMetrics(const FontDisplayMetrics& value) {
  if(!value.ready||!value.revision||value.revision>9007199254740991ULL||value.displayId>9007199254740991ULL)
    throw std::runtime_error("Actual main-window display metrics are unavailable; no fixed-DPI fallback");
  ValidateFontDisplayObservation(static_cast<double>(value.displayId),value.dpiX,value.dpiY,value.densityPixels);
}
class FontDisplayMetricsStore {
  FontDisplayMetrics value_;
  uint64_t Next()const {
    if(value_.revision>=9007199254740991ULL)throw std::overflow_error("Font display revision exhausted");
    return value_.revision+1;
  }
public:
  const FontDisplayMetrics& Get()const {return value_;}
  FontDisplayMetrics Set(double id,double x,double y,double density) {
    ValidateFontDisplayObservation(id,x,y,density);
    if(value_.ready&&value_.displayId==static_cast<uint64_t>(id)&&value_.dpiX==x&&value_.dpiY==y&&value_.densityPixels==density)return value_;
    const auto revision=Next();
    value_={true,static_cast<uint64_t>(id),revision,x,y,density};return value_;
  }
  FontDisplayMetrics Clear() {
    if(value_.ready){const auto revision=Next();value_={};value_.revision=revision;}
    return value_;
  }
};
struct DisplayRasterParameters {float pixelSize=0,scaleX=0;};
inline DisplayRasterParameters RasterForDisplay(unsigned points,const FontDisplayMetrics& display) {
  RequireFontDisplayMetrics(display);
  if(points<1||points>256)throw std::invalid_argument("Effective font point size is outside [1,256]");
  const double pixels=points*display.dpiY/72.0;
  if(!std::isfinite(pixels)||pixels<1||pixels>512)
    throw std::invalid_argument("Actual-DPI raster exceeds existing bounded pixel size [1,512]");
  return {static_cast<float>(pixels),static_cast<float>(display.dpiX/display.dpiY)};
}
inline std::string FontDisplayMetricsJson(const FontDisplayMetrics& value) {
  std::ostringstream out;out<<std::setprecision(17)<<"{\"ready\":"<<(value.ready?"true":"false")
    <<",\"display_id\":"<<value.displayId<<",\"revision\":"<<value.revision
    <<",\"dpi_x\":"<<value.dpiX<<",\"dpi_y\":"<<value.dpiY<<",\"density_pixels\":"<<value.densityPixels
    <<",\"source\":\"actual-main-window-display-xDPI-yDPI\",\"fixed_dpi_fallback\":false}";
  return out.str();
}
inline bool SameFontDisplayMetrics(const FontDisplayMetrics& a,const FontDisplayMetrics& b) {
  return a.ready==b.ready&&a.displayId==b.displayId&&a.revision==b.revision&&
    a.dpiX==b.dpiX&&a.dpiY==b.dpiY&&a.densityPixels==b.densityPixels;
}
}
