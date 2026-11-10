#pragma once
// Deliberately minimal modeled base for unit testing the platform lease state
// machine only. Not original scene/GL evidence and never part of the HAP.
#include <string>
using olxstr=std::string;
class TGlFont {};
class AGlScene {
  bool enabled_=false;
protected:
  virtual TGlFont& DoCreateFont(TGlFont&,bool) const=0;
public:
  virtual ~AGlScene()=default;
  void SetEnabled(bool enabled){enabled_=enabled;}
  bool IsEnabled()const{return enabled_;}
  virtual bool MakeCurrent()=0;
  virtual bool StartDraw(){return MakeCurrent();}
  virtual void EndDraw(){}
  virtual void ScaleFonts(double)=0;
  virtual void RestoreFontScale()=0;
  virtual olxstr ShowFontDialog(TGlFont*,const olxstr&)=0;
  void RequestModeledFont(){TGlFont font;DoCreateFont(font,false);}
};
