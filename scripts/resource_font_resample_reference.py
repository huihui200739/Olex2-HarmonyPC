"""Independent font metrics/layout on fixed wx source RGB output, never emulator control."""
from native_font_progress_validation import require
from native_font_raster_window_validation import raster_position,f32
CASES=(('small','nr__',7),('large','nrb_',31),('large-scaled','nrb_',46),('large-restored','nrb_',31),('fixed-italic','fib_',96),('identity','ni__',72))
def inspect_resource_rgb(raw,width,height,fixed):
 require(type(width) is int and type(height) is int and 0<width<=2048 and 0<height<=2048,'Invalid resource RGB dimensions')
 cell=width*height*3;require(len(raw)==cell*256 and len(raw)<=64*1024*1024,'Exact bounded256 resource cells required')
 masks=[];metrics=[];max_width=max_height=0;leftmost=1000
 for code in range(256):
  points=[];start=code*cell;background=raw[start+cell-1]
  for y in range(height):
   for x in range(width):
    at=start+(y*width+x)*3
    if (raw[at]|raw[at+1]|raw[at+2])!=background:points.append((x,y))
  masks.append(points)
  if points:
   xs,ys=zip(*points);left,top,right,bottom=min(xs),min(ys),max(xs),max(ys)
   metrics.append([left,top,right,bottom]);max_width=max(max_width,right-left);max_height=max(max_height,bottom);leftmost=min(leftmost,left)
  else:metrics.append(None)
 require(max_width>0 and max_height>0,'Resource RGB contains no bounded geometry')
 offset=2+max_width//10;blank=max_width if fixed else min((max_width//8+1)*8,offset*5)
 for code,glyph in enumerate(metrics):
  if glyph is None:metrics[code]=[0,max_height//2,blank,max_height//2]
 text_width=max_width*4 if fixed else sum(metrics[c][2]+offset for c in b'Ab0')+metrics[ord('9')][2]
 return dict(masks=masks,glyphs=metrics,max_width=max_width,max_height=max_height,leftmost=leftmost,char_offset=offset,text_width=text_width,fixed=fixed)
def resource_rgb_frame(reference,case,width,height):
 require(case['text']=='Ab09' and case['fixed'] is reference['fixed'],'Original resource RGB text/fixed contract changed')
 x,y=raster_position(case,width,height);out=bytearray(b'\xff\xff\xff\xff'*(width*height));ink=0
 for code in case['text'].encode('ascii'):
  for sx,sy in reference['masks'][code]:
   bitmap_y=reference['max_height']-sy
   if not (0<=bitmap_y<reference['max_height']):continue
   px=int(x)+sx-reference['leftmost'];py=int(y)+bitmap_y
   require(0<=px<width and 0<=py<height,'Original resource RGB text would be clipped')
   at=(py*width+px)*4;out[at:at+4]=b'\xff\x00\x00\xff';ink+=1
  advance=reference['max_width'] if reference['fixed'] else reference['glyphs'][code][2]+reference['char_offset']
  x=f32(x+advance)
 return bytes(out),ink
