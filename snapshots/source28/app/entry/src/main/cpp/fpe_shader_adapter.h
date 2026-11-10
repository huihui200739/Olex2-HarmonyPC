#pragma once
#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_map>
// Feasibility-only ESSL100 -> ESSL300 translation of pinned GL4ES FPE output.
// No user shader API and no claim of general GLSL translation.
inline std::string AdaptGeneratedFPE(const std::string& input,bool vertex){
 if(input.find("// FPE_Shader generated")==std::string::npos)return input;
 if(input.rfind("#version 100\n",0)!=0)throw std::runtime_error("Unexpected pinned FPE version");
 const std::unordered_map<std::string,std::string> identifiers={
  {"attribute","in"},{"varying",vertex?"out":"in"},
  {"gl_FragColor","_olex2_FragColor"},{"gl_FragDepthEXT","gl_FragDepth"},
  {"texture2D","texture"},{"textureCube","texture"},
  {"texture2DProj","textureProj"},{"texture2DLod","textureLod"}};
 std::string out="#version 300 es\n";
 for(size_t i=13;i<input.size();){
  if(input.compare(i,2,"//")==0){size_t end=input.find('\n',i);if(end==std::string::npos)end=input.size();out.append(input,i,end-i);i=end;continue;}
  if(input.compare(i,2,"/*")==0){size_t end=input.find("*/",i+2);if(end==std::string::npos)throw std::runtime_error("Unterminated FPE comment");end+=2;out.append(input,i,end-i);i=end;continue;}
  const unsigned char ch=input[i];
  if(std::isalpha(ch)||ch=='_'){
   size_t end=i+1;while(end<input.size()&&(std::isalnum(static_cast<unsigned char>(input[end]))||input[end]=='_'))++end;
   const std::string token=input.substr(i,end-i);
   if(token=="gl_FragData")throw std::runtime_error("FPE multiple fragment outputs unsupported by prototype");
   auto found=identifiers.find(token);out+=found==identifiers.end()?token:found->second;i=end;
  }else{out+=input[i++];}
 }
 if(!vertex){size_t main=out.find("void main");if(main==std::string::npos)throw std::runtime_error("Pinned FPE main missing");out.insert(main,"out vec4 _olex2_FragColor;\n");}
 return out;
}
