#!/usr/bin/env python3
"""Extract unchanged fixed wx3.2.6 RGB resampling methods for a host-only reference.
The tiny image container is NOT wxWidgets runtime/GUI/device execution.
"""
import argparse
import hashlib
import json
from pathlib import Path
EXPECTED='41c8a0bcad8487b6c1ac0d9758cbd92b3c7b3d72499331de8d4f844cf5afcb39'
PRELUDE=r'''// Host-only source-method reference. Not packaged in the Harmony application.
// Original methods: Copyright (c) Robert Roebling, wxWindows licence.
#include <cassert>
#include <memory>
#include <vector>
#define wxASSERT(x) assert(x)
#define wxCHECK_MSG(x,retval,message) do { if(!(x)) return retval; } while(0)
#define wxS(x) x
template<class T>using wxVector=std::vector<T>;
class wxImage {
 struct Data {int m_width=0,m_height=0;std::vector<unsigned char> rgb,alpha;unsigned char* m_data=nullptr;unsigned char* m_alpha=nullptr;};
 std::shared_ptr<Data> image_;
 public:
 wxImage()=default;
 wxImage(int w,int h,bool clear){Create(w,h,clear);}
 void Create(int w,int h,bool){image_=std::make_shared<Data>();image_->m_width=w;image_->m_height=h;image_->rgb.resize(w*h*3);image_->m_data=image_->rgb.data();}
 unsigned char* GetData()const{return image_->m_data;}
 void SetAlpha(){image_->alpha.resize(image_->m_width*image_->m_height);image_->m_alpha=image_->alpha.data();}
 unsigned char* GetAlpha()const{return image_->m_alpha;}
 wxImage ResampleBox(int width,int height)const;
 wxImage ResampleBicubic(int width,int height)const;
};
#define M_IMGDATA (image_.get())
'''
END=r'''
std::vector<unsigned char> OriginalWxHigh(const std::vector<unsigned char>& source,unsigned ow,unsigned oh,unsigned w,unsigned h){
 if(ow==w&&oh==h)return source;
 wxImage image(ow,oh,false);std::copy(source.begin(),source.end(),image.GetData());
 const auto scaled=w<ow&&h<oh?image.ResampleBox(w,h):image.ResampleBicubic(w,h);
 return {scaled.GetData(),scaled.GetData()+w*h*3};
}
'''
MARKERS=('struct BoxPrecalc','void ResampleBoxPrecalc(', 'wxImage wxImage::ResampleBox(',
 'static inline double spline_cube(', 'static inline double spline_weight(',
 'struct BicubicPrecalc', 'inline void DoCalc(BicubicPrecalc&', 'void ResampleBicubicPrecalc(', 'wxImage wxImage::ResampleBicubic(')
def extract(text,marker):
 if text.count(marker)!=1:raise ValueError('Changed fixed wx reference marker: '+marker)
 start=text.index(marker);opening=text.index('{',start);depth=1;end=opening+1
 while depth:
  if text[end]=='{':depth+=1
  elif text[end]=='}':depth-=1
  end+=1
 if marker.startswith('struct '):
  if text[end]!=';':raise ValueError('Unexpected fixed reference struct ending')
  end+=1
 return text[start:end]
def prepare(source,out):
 raw=source.read_bytes()
 if hashlib.sha256(raw).hexdigest()!=EXPECTED:raise ValueError('Only exact retained original wx3.2.6 image.cpp accepted')
 text=raw.decode();methods=[extract(text,marker) for marker in MARKERS]
 out.mkdir(parents=True,exist_ok=False)
 result=PRELUDE+'\n\n'.join(methods)+END
 (out/'original-wx-rgb-methods.cpp').write_text(result)
 manifest=dict(source_sha256=EXPECTED,upstream='https://raw.githubusercontent.com/wxWidgets/wxWidgets/v3.2.6/src/common/image.cpp',
  extracted_methods=[dict(marker=marker,sha256=hashlib.sha256(method.encode()).hexdigest()) for marker,method in zip(MARKERS,methods)],
  generated_sha256=hashlib.sha256(result.encode()).hexdigest(),original_method_bodies_unmodified=True,
  shim='Minimal RGB image storage only; not a full wxImage binary/runtime',alpha_mask_gui_qualification=False,
  packaged_in_hap=False,device_executed=False,full_port_complete=False)
 (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');return manifest
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--source',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();print(json.dumps(prepare(args.source,args.output),indent=2))
