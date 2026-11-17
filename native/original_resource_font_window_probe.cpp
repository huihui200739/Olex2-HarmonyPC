#include "original_resource_font_window_probe.h"
#include "olex_font_resource_fixture.h"
#include "bapp.h"
#include "eutf8.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
namespace olex2::harmony {
namespace {
void Check(const char* stage){const auto e=glGetError();if(e)throw std::runtime_error(std::string(stage)+" GL="+std::to_string(e));}
void Directory(const std::string& path){struct stat st{};if(mkdir(path.c_str(),0700)!=0&&errno!=EEXIST)throw std::runtime_error("Cannot create private font fixture directory");if(lstat(path.c_str(),&st)!=0||!S_ISDIR(st.st_mode))throw std::runtime_error("Private font fixture directory is not an ordinary directory");}
std::vector<unsigned char> Read(const std::string& path){std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot read private font fixture");return {std::istreambuf_iterator<char>(in),{}};}
void Save(const std::string& path,const std::vector<unsigned char>& data){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),data.size());out.close();if(!out)throw std::runtime_error("Cannot save resource font observation");}
std::string EnsureFixture(){
 const std::string base=TUtf8::Encode(TBasicApp::GetBaseDir()).c_str();Directory(base+"etc");Directory(base+"etc/Fonts");
 const std::string path=base+"etc/Fonts/harmony-synthetic-resource.fnt";const std::vector<unsigned char> expected(std::begin(OlexFontResourceFixture),std::end(OlexFontResourceFixture));
 const int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
 if(fd>=0){size_t written=0;while(written<expected.size()){const ssize_t n=write(fd,expected.data()+written,expected.size()-written);if(n<0&&errno==EINTR)continue;if(n<=0){close(fd);throw std::runtime_error("Cannot finish private font fixture");}written+=static_cast<size_t>(n);}const int flushed=fsync(fd),closed=close(fd);if(flushed!=0||closed!=0)throw std::runtime_error("Cannot flush private font fixture");}
 else if(errno!=EEXIST)throw std::runtime_error("Cannot create private font fixture");
 if(Read(path)!=expected)throw std::runtime_error("Existing font fixture differs; not overwriting it");return path;
}
void Setup(int w,int h){glViewport(0,0,w,h);glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_LIGHTING);glDisable(GL_TEXTURE_1D);glDisable(GL_TEXTURE_2D);glDisable(GL_BLEND);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,w,0,h,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(1,1,1,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glColor4f(1,0,0,1);}
void Snapshot(std::ostringstream& out,const Olex2LegacyHost& host,PlatformWindowScene& scene,TGlFont& font,int w,int h,unsigned index,const char* label){
 Setup(w,h);short state=0;glRasterPos2i(32,32);font.DrawRasterText(olxstr("Ab09"),state);glFinish();Check("original resource font draw");
 float projection[16]={},modelview[16]={};glGetFloatv(GL_PROJECTION_MATRIX,projection);glGetFloatv(GL_MODELVIEW_MATRIX,modelview);
 std::vector<unsigned char> actual(static_cast<size_t>(w)*h*4);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());Check("original resource font readback");
 const std::string prefix=std::string("resource-")+label;Save(std::string(host.files_dir)+"/"+prefix+"-actual.rgba",actual);
 const auto& record=scene.FontRecord(font);const auto& data=record.resourceData;Save(std::string(host.files_dir)+"/"+prefix+"-bits.bin",data.bits);
 if(host.gpu_snapshot)host.gpu_snapshot(host.run,prefix.c_str());
 if(index)out<<',';out<<std::setprecision(17)<<"{\"label\":\""<<label<<"\",\"font_address\":"<<reinterpret_cast<uintptr_t>(&font)
  <<",\"font_created\":"<<(font.IsCreated()?"true":"false")<<",\"is_vector\":"<<(font.IsVectorFont()?"true":"false")<<",\"original_resource\":"<<(record.resource?"true":"false")
  <<",\"point_size\":"<<font.GetPointSize()<<",\"entry\":\""<<data.entry<<"\",\"width\":"<<data.width<<",\"height\":"<<data.height<<",\"compression\":"<<data.compression<<",\"crc\":"<<data.crc
  <<",\"max_width\":"<<font.GetMaxWidth()<<",\"max_height\":"<<font.GetMaxHeight()<<",\"leftmost\":"<<font.GetLeftmost()<<",\"char_offset\":"<<font.GetCharOffset()<<",\"text_width\":"<<font.TextWidth(olxstr("Ab09"))
  <<",\"builds\":"<<record.builds<<",\"releases\":"<<record.releases<<",\"resource_reads\":"<<record.resourceReads<<",\"rasterizations\":"<<record.rasterizations
  <<",\"origin\":[32,32],\"text\":\"Ab09\",\"fixed\":false,\"draw_state_after\":"<<state<<",\"glyphs\":[";
 for(unsigned i=0;i<256;++i){const auto* c=font.CharSize(i);if(i)out<<',';out<<'['<<c->Left<<','<<c->Top<<','<<c->Right<<','<<c->Bottom<<']';}
 out<<"],\"projection\":[";for(unsigned i=0;i<16;++i){if(i)out<<',';out<<projection[i];}out<<"],\"modelview\":[";for(unsigned i=0;i<16;++i){if(i)out<<',';out<<modelview[i];}out<<"]}";
}
}
std::string RunOriginalResourceFontWindowProbe(const Olex2LegacyHost& host,PlatformWindowScene& scene,int w,int h){
 const auto path=EnsureFixture();Save(std::string(host.files_dir)+"/resource-font-archive.fnt",Read(path));
 static TGlFont* registered=nullptr;if(!registered)registered=&scene.CreateFont("HarmonyOriginalResourceProbe",olxstr("#harmony-synthetic-resource.fnt:nr__18"));
 auto& normal=scene.GetFont(registered->GetId(),false);std::ostringstream out;
 out<<"{\"description\":\"#harmony-synthetic-resource.fnt:nr__18\",\"synthetic_fixture\":true,\"genuine_CreateGlyphs\":true,\"default_font_fallback\":false,\"cases\":[";
 Snapshot(out,host,scene,normal,w,h,0,"normal");scene.ScaleFonts(1.5);
 try{auto& scaled=scene.GetFont(registered->GetId(),false);Snapshot(out,host,scene,scaled,w,h,1,"scaled");}catch(...){scene.RestoreFontScale();throw;}
 scene.RestoreFontScale();auto& restored=scene.GetFont(registered->GetId(),false);Snapshot(out,host,scene,restored,w,h,2,"restored");
 out<<"],\"font_object_preserved\":"<<(&normal==&restored?"true":"false")<<",\"execution_completed\":true}";return out.str();
}
}
