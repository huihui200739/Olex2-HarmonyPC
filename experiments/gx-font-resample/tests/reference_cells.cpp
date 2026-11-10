// Host-only reference: invokes unchanged fixed wx source methods, not the new resampler.
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
std::vector<unsigned char> OriginalWxHigh(const std::vector<unsigned char>&,unsigned,unsigned,unsigned,unsigned);
unsigned Dimension(const char* raw){std::string s(raw);unsigned n=0;if(s.empty())throw std::invalid_argument("empty dimension");for(char c:s){if(c<'0'||c>'9'||n>2048)throw std::invalid_argument("dimension");n=n*10+unsigned(c-'0');}if(n==0||n>2048)throw std::invalid_argument("dimension range");return n;}
int main(int argc,char** argv){
 if(argc!=7)throw std::invalid_argument("reference_cells old-width old-height width height source.rgb output.rgb");
 const unsigned ow=Dimension(argv[1]),oh=Dimension(argv[2]),w=Dimension(argv[3]),h=Dimension(argv[4]);
 const size_t cell=static_cast<size_t>(ow)*oh*3,target=static_cast<size_t>(w)*h*3;
 if(cell*256>128u*1024u*1024u||target*256>64u*1024u*1024u)throw std::invalid_argument("bounded font cache exceeded");
 std::ifstream input(argv[5],std::ios::binary);if(!input)throw std::runtime_error("reference input unavailable");
 const std::vector<unsigned char> raw(std::istreambuf_iterator<char>(input),{});if(raw.size()!=cell*256)throw std::invalid_argument("exact256 RGB cells required");
 if(std::filesystem::exists(argv[6]))throw std::runtime_error("reference output already exists; not overwriting");
 std::ofstream output(argv[6],std::ios::binary);if(!output)throw std::runtime_error("reference output unavailable");
 for(unsigned c=0;c<256;++c){const std::vector<unsigned char> source(raw.begin()+c*cell,raw.begin()+(c+1)*cell);const auto scaled=OriginalWxHigh(source,ow,oh,w,h);output.write(reinterpret_cast<const char*>(scaled.data()),scaled.size());}
 output.close();if(!output)throw std::runtime_error("reference output incomplete");
 std::cout<<"fixed wx source reference wrote "<<target*256<<" RGB bytes; no GL/device execution\n";
}
