#include "renderer.h"
#include "utility_checks.h"
#include "application_runtime.h"
#include "original_interpreter.h"
#include "eutf8.h"
#include <memory>
#include <mutex>
#include <cstring>
#include <napi/native_api.h>
#include <hilog/log.h>
#include <cmath>
#include <string>
#include <vector>

namespace {
constexpr unsigned DOMAIN = 0x3801;
napi_value String(napi_env env, const std::string& value) { napi_value out; napi_create_string_utf8(env,value.c_str(),value.size(),&out); return out; }
void Property(napi_env env,napi_value object,const char* key,napi_value value) { napi_set_named_property(env,object,key,value); }
void Boolean(napi_env env,napi_value object,const char* key,bool value) { napi_value v; napi_get_boolean(env,value,&v); Property(env,object,key,v); }
void Number(napi_env env,napi_value object,const char* key,double value) { napi_value v; napi_create_double(env,value,&v); Property(env,object,key,v); }
struct RuntimeOwner {std::mutex mutex;std::unique_ptr<olex2::harmony::ApplicationRuntime> runtime;std::string filesDir;};
RuntimeOwner& Runtime(){static RuntimeOwner owner;return owner;}
napi_value RuntimeObject(napi_env env,const olex2::harmony::ApplicationRuntimeStatus& s){
  napi_value out;napi_create_object(env,&out);
  Boolean(env,out,"initialized",s.initialized);Boolean(env,out,"optionsRestored",s.optionsRestored);Boolean(env,out,"licenseVerified",s.licenseVerified);Boolean(env,out,"commandHints",s.commandHints);Boolean(env,out,"lastSaveCommitted",s.lastSaveCommitted);
  Boolean(env,out,"originalCoreInitialized",s.originalCoreInitialized);Boolean(env,out,"originalCoreEmptyFile",s.originalCoreEmptyFile);
  Number(env,out,"originalCoreMacroCount",s.originalCoreMacroCount);Number(env,out,"originalCoreFunctionCount",s.originalCoreFunctionCount);Number(env,out,"originalCoreFileCount",s.originalCoreFileCount);
  Number(env,out,"originalFunctionCount",s.originalFunctionCount);Number(env,out,"optionsCount",s.optionsCount);Number(env,out,"saveRevision",s.saveRevision);
  Property(env,out,"baseDir",String(env,s.baseDir));Property(env,out,"instanceDir",String(env,s.instanceDir));Property(env,out,"sharedDir",String(env,s.sharedDir));Property(env,out,"configDir",String(env,s.configDir));Property(env,out,"licenseSha256",String(env,s.licenseSha256));Property(env,out,"optionsSha256",String(env,s.optionsSha256));Property(env,out,"platform",String(env,s.platform));Property(env,out,"error",String(env,s.error));Property(env,out,"source",String(env,olex2::harmony::ApplicationRuntimeSource()));return out;
}
bool ReadString(napi_env env,napi_value value,size_t maximum,std::string& output){size_t size=0;if(napi_get_value_string_utf8(env,value,nullptr,0,&size)!=napi_ok || size==0 || size>maximum)return false;std::vector<char>b(size+1);if(napi_get_value_string_utf8(env,value,b.data(),b.size(),&size)!=napi_ok)return false;output.assign(b.data(),size);return output.find('\0')==std::string::npos;}
napi_value InitializeApplicationRuntime(napi_env env,napi_callback_info info){
  napi_value args[2];size_t count=2;napi_get_cb_info(env,info,&count,args,nullptr,nullptr);
  std::string root;napi_typedarray_type type;size_t length=0,offset=0;void* data=nullptr;napi_value arraybuffer;
  if(count!=2 || !ReadString(env,args[0],4096,root) || napi_get_typedarray_info(env,args[1],&type,&length,&data,&arraybuffer,&offset)!=napi_ok || type!=napi_uint8_array || !data || length==0 || length>65536){napi_throw_type_error(env,nullptr,"Expected ability filesDir and packaged original licence Uint8Array");return nullptr;}
  auto& owner=Runtime();std::lock_guard<std::mutex> lock(owner.mutex);olex2::harmony::ApplicationRuntimeStatus status;
  try{if(owner.runtime){if(root!=owner.filesDir)throw std::runtime_error("Cannot rebind a running original application runtime");}else{owner.runtime.reset(new olex2::harmony::ApplicationRuntime(root,std::string(static_cast<char*>(data),length)));owner.filesDir=root;}status=owner.runtime->Status();}
  catch(const TExceptionBase& e){status.error=TUtf8::Encode(e.GetException()->GetFullMessage()).c_str();}catch(const std::exception& e){status.error=e.what();}
  return RuntimeObject(env,status);
}
napi_value GetApplicationRuntime(napi_env env,napi_callback_info){auto& owner=Runtime();std::lock_guard<std::mutex> lock(owner.mutex);olex2::harmony::ApplicationRuntimeStatus status;try{if(owner.runtime)status=owner.runtime->Status();else status.error="Original application runtime is not initialized";}catch(const std::exception& e){status.error=e.what();}return RuntimeObject(env,status);}
napi_value ShutdownApplicationRuntime(napi_env env,napi_callback_info){auto& owner=Runtime();std::lock_guard<std::mutex> lock(owner.mutex);olex2::harmony::OriginalInterpreterGuard guard;owner.runtime.reset();owner.filesDir.clear();napi_value out;napi_get_undefined(env,&out);return out;}
napi_value EvaluateApplicationFunction(napi_env env,napi_callback_info info){
  size_t count=1;napi_value args[1];napi_get_cb_info(env,info,&count,args,nullptr,nullptr);std::string input;if(count!=1 || !ReadString(env,args[0],4096,input)){napi_throw_type_error(env,nullptr,"Expected an original application function expression");return nullptr;}
  auto& owner=Runtime();std::lock_guard<std::mutex> lock(owner.mutex);olex2::harmony::ApplicationFunctionResult result;result.input=input;
  try{if(!owner.runtime)throw std::runtime_error("Original application runtime is not initialized");result=owner.runtime->Evaluate(input);}catch(const std::exception& e){result.error=e.what();}
  napi_value out;napi_create_object(env,&out);Boolean(env,out,"passed",result.passed);Boolean(env,out,"optionsChanged",result.optionsChanged);Property(env,out,"input",String(env,result.input));Property(env,out,"text",String(env,result.text));Property(env,out,"error",String(env,result.error));Property(env,out,"source",String(env,olex2::harmony::ApplicationRuntimeSource()));Property(env,out,"runtime",RuntimeObject(env,result.runtime));return out;
}
napi_value OriginalLogsObject(napi_env env,const olex2::harmony::OriginalLogSnapshot& s,const std::string& error){
  napi_value out,entries;napi_create_object(env,&out);napi_create_array_with_length(env,s.entries.size(),&entries);
  Boolean(env,out,"attached",s.attached);Number(env,out,"posted",s.posted);Number(env,out,"dropped",s.dropped);Number(env,out,"truncated",s.truncated);Number(env,out,"captureFailures",s.captureFailures);Number(env,out,"clearCount",s.clearCount);Number(env,out,"clearedEntries",s.clearedEntries);Number(env,out,"entryCount",s.entries.size());Number(env,out,"retainedBytes",s.retainedBytes);
  Property(env,out,"textSha256",String(env,s.textSha256));Property(env,out,"source",String(env,olex2::harmony::OriginalLogSource()));Property(env,out,"error",String(env,error));
  for(size_t i=0;i<s.entries.size();++i){napi_value e;napi_create_object(env,&e);Number(env,e,"sequence",s.entries[i].sequence);Boolean(env,e,"truncated",s.entries[i].truncated);Property(env,e,"text",String(env,s.entries[i].text));napi_set_element(env,entries,i,e);}
  Property(env,out,"entries",entries);return out;
}
napi_value ReadOriginalLogs(napi_env env,bool clear){
  auto& owner=Runtime();std::lock_guard<std::mutex> lock(owner.mutex);olex2::harmony::OriginalLogSnapshot snapshot;std::string error;
  try{if(!owner.runtime)throw std::runtime_error("Original application runtime is not initialized");snapshot=clear?owner.runtime->ClearOriginalLogs():owner.runtime->OriginalLogs();}
  catch(const TExceptionBase& e){error=TUtf8::Encode(e.GetException()->GetFullMessage()).c_str();}catch(const std::exception& e){error=e.what();}
  return OriginalLogsObject(env,snapshot,error);
}
napi_value GetOriginalLogs(napi_env env,napi_callback_info){return ReadOriginalLogs(env,false);}
napi_value ClearOriginalLogs(napi_env env,napi_callback_info){return ReadOriginalLogs(env,true);}
napi_value RunCoreCatalog(napi_env env,napi_callback_info info){
  size_t count=1;napi_value args[1];napi_get_cb_info(env,info,&count,args,nullptr,nullptr);std::string input;
  if(count!=1 || !ReadString(env,args[0],2048,input)){napi_throw_type_error(env,nullptr,"Expected a bounded original core catalog command");return nullptr;}
  auto& owner=Runtime();std::lock_guard<std::mutex> lock(owner.mutex);olex2::harmony::CoreCatalogResult r;r.input=input;
  try{if(!owner.runtime)throw std::runtime_error("Original application runtime is not initialized");r=owner.runtime->RunCoreCatalog(input);}catch(const std::exception& e){r.error=e.what();}
  napi_value out;napi_create_object(env,&out);Boolean(env,out,"passed",r.passed);Number(env,out,"exposedMacros",r.exposedMacros);Number(env,out,"postedBefore",r.postedBefore);Number(env,out,"postedAfter",r.postedAfter);Property(env,out,"input",String(env,r.input));Property(env,out,"error",String(env,r.error));Property(env,out,"source",String(env,olex2::harmony::CoreCatalogSource()));return out;
}
napi_value OptionsTransferObject(napi_env env,const olex2::harmony::ApplicationOptionsTransferResult& result){
  napi_value out,buffer,array;napi_create_object(env,&out);void* data=nullptr;
  napi_create_arraybuffer(env,result.bytes.size(),&data,&buffer);
  if(!result.bytes.empty())std::memcpy(data,result.bytes.data(),result.bytes.size());
  napi_create_typedarray(env,napi_uint8_array,result.bytes.size(),buffer,0,&array);
  Boolean(env,out,"passed",result.passed);Boolean(env,out,"optionsChanged",result.optionsChanged);
  Number(env,out,"importedOptions",result.importedOptions);Property(env,out,"data",array);
  Property(env,out,"sha256",String(env,result.sha256));Property(env,out,"error",String(env,result.error));
  Property(env,out,"runtime",RuntimeObject(env,result.runtime));return out;
}
napi_value ExportApplicationOptions(napi_env env,napi_callback_info){
  auto& owner=Runtime();std::lock_guard<std::mutex> lock(owner.mutex);olex2::harmony::ApplicationOptionsTransferResult result;
  try{if(!owner.runtime)throw std::runtime_error("Original application runtime is not initialized");result=owner.runtime->ExportOptions();}
  catch(const TExceptionBase& e){result.error=TUtf8::Encode(e.GetException()->GetFullMessage()).c_str();}catch(const std::exception& e){result.error=e.what();}
  return OptionsTransferObject(env,result);
}
napi_value ImportApplicationOptions(napi_env env,napi_callback_info info){
  size_t count=1;napi_value args[1];napi_get_cb_info(env,info,&count,args,nullptr,nullptr);
  napi_typedarray_type type;size_t length=0,offset=0;void* data=nullptr;napi_value buffer;
  if(count!=1 || napi_get_typedarray_info(env,args[0],&type,&length,&data,&buffer,&offset)!=napi_ok || type!=napi_uint8_array || !data || length==0 || length>olex2::harmony::MaximumOptionsTransferBytes){napi_throw_type_error(env,nullptr,"Expected original-format settings Uint8Array, 1 byte to 1 MiB");return nullptr;}
  auto& owner=Runtime();std::lock_guard<std::mutex> lock(owner.mutex);olex2::harmony::ApplicationOptionsTransferResult result;
  try{if(!owner.runtime)throw std::runtime_error("Original application runtime is not initialized");result=owner.runtime->ImportOptions(std::string(static_cast<char*>(data),length));}
  catch(const TExceptionBase& e){result.error=TUtf8::Encode(e.GetException()->GetFullMessage()).c_str();}catch(const std::exception& e){result.error=e.what();}
  return OptionsTransferObject(env,result);
}
napi_value LinePathProbeObject(napi_env env,const olex2::harmony::LinePathGpuProbe& p) {
  napi_value out;napi_create_object(env,&out);Boolean(env,out,"verified",p.verified);Boolean(env,out,"stateRestored",p.stateRestored);Boolean(env,out,"contextRestored",p.contextRestored);
  Property(env,out,"source",String(env,olex2::harmony::OriginalLinePathGpuProbeSource()));Property(env,out,"error",String(env,p.error));
  napi_value cases;napi_create_array_with_length(env,p.cases.size(),&cases);
  for(size_t i=0;i<p.cases.size();++i){const auto& c=p.cases[i];napi_value item;napi_create_object(env,&item);
    Property(env,item,"kind",String(env,c.kind));Property(env,item,"indexSha256",String(env,c.indexSha256));Number(env,item,"inputVertices",c.inputVertices);Number(env,item,"indexCount",c.indexCount);Number(env,item,"actualLineWidth",c.actualLineWidth);
    Boolean(env,item,"actualBufferVerified",c.actualBufferVerified);Boolean(env,item,"pixelsVerified",c.pixelsVerified);
    napi_value indices;napi_create_array_with_length(env,c.actualIndices.size(),&indices);for(size_t j=0;j<c.actualIndices.size();++j){napi_value v;napi_create_uint32(env,c.actualIndices[j],&v);napi_set_element(env,indices,j,v);}Property(env,item,"actualIndices",indices);
    napi_value samples;napi_create_array_with_length(env,c.samples.size(),&samples);
    for(size_t j=0;j<c.samples.size();++j){const auto& sample=c.samples[j];napi_value node;napi_create_object(env,&node);Property(env,node,"name",String(env,sample.name));Number(env,node,"x",sample.x);Number(env,node,"y",sample.y);Boolean(env,node,"expectedLit",sample.expectedLit);Boolean(env,node,"passed",sample.passed);
      napi_value rgba;napi_create_array_with_length(env,sample.rgba.size(),&rgba);for(size_t k=0;k<sample.rgba.size();++k){napi_value v;napi_create_uint32(env,sample.rgba[k],&v);napi_set_element(env,rgba,k,v);}Property(env,node,"rgba",rgba);napi_set_element(env,samples,j,node);}
    Property(env,item,"samples",samples);napi_set_element(env,cases,i,item);
  }
  Property(env,out,"cases",cases);return out;
}
napi_value Graphics(napi_env env,napi_callback_info) {
  const auto status=Renderer::Instance().Status(); napi_value out; napi_create_object(env,&out);
  Property(env,out,"linePathProbe",LinePathProbeObject(env,status.linePathProbe));
  Boolean(env,out,"surfaceReady",status.surfaceReady); Boolean(env,out,"initialized",status.initialized);
  Boolean(env,out,"pixelVerified",status.pixelVerified); Boolean(env,out,"geometryVerified",status.geometryVerified);
  Boolean(env,out,"coverageVerified",status.coverageVerified); Boolean(env,out,"meshValidated",status.meshValidated);
  Boolean(env,out,"submissionValidated",status.submissionValidated);
  Number(env,out,"submittedVertices",status.submittedVertices);
  Boolean(env,out,"originalTriangleVerified",status.originalTriangleVerified);
  Number(env,out,"originalTriangleVertices",status.originalTriangleVertices);
  Number(env,out,"originalTriangleNormalCommands",status.originalTriangleNormalCommands);
  Number(env,out,"originalTriangleColorCommands",status.originalTriangleColorCommands);
  Property(env,out,"originalTriangleSource",String(env,status.originalTriangleSource));
  Boolean(env,out,"originalLineVerified",status.originalLineVerified);Boolean(env,out,"originalLineBufferVerified",status.originalLineBufferVerified);
  Number(env,out,"originalLineVertices",status.originalLineVertices);Number(env,out,"originalLineColorCommands",status.originalLineColorCommands);
  Number(env,out,"originalLineWidthQueries",status.originalLineWidthQueries);Number(env,out,"originalLineWidthCommands",status.originalLineWidthCommands);
  Property(env,out,"originalLineSource",String(env,status.originalLineSource));Property(env,out,"originalLineIndexSha256",String(env,status.originalLineIndexSha256));
  Boolean(env,out,"originalPolygonVerified",status.originalPolygonVerified);
  Number(env,out,"originalPolygonBatches",status.originalPolygonBatches);Number(env,out,"originalPolygonVertices",status.originalPolygonVertices);
  Number(env,out,"originalPolygonNormalCommands",status.originalPolygonNormalCommands);Number(env,out,"originalPolygonColorCommands",status.originalPolygonColorCommands);
  Property(env,out,"originalPolygonSource",String(env,status.originalPolygonSource));
  Boolean(env,out,"polygonProbeContextRestored",status.polygonProbe.contextRestored);
  Boolean(env,out,"polygonProbeStateRestored",status.polygonProbe.stateRestored);
  Boolean(env,out,"polygonProbeVerified",status.polygonProbe.verified);Property(env,out,"polygonProbeSource",String(env,PolygonGpuProbeSource()));
  Number(env,out,"polygonProbeInputVertices",status.polygonProbe.inputVertices);Number(env,out,"polygonProbeTriangles",status.polygonProbe.triangles);
  Number(env,out,"polygonProbeNormalCommands",status.polygonProbe.normalCommands);Number(env,out,"polygonProbeColorCommands",status.polygonProbe.colorCommands);
  napi_value polygonSamples;napi_create_array_with_length(env,status.polygonProbe.samples.size(),&polygonSamples);
  for(unsigned i=0;i<status.polygonProbe.samples.size();++i) {
    napi_value tuple;napi_create_array_with_length(env,7,&tuple);
    for(unsigned j=0;j<7;++j) {napi_value value;napi_create_uint32(env,status.polygonProbe.samples[i][j],&value);napi_set_element(env,tuple,j,value);}
    napi_set_element(env,polygonSamples,i,tuple);
  }
  Property(env,out,"polygonProbeSamples",polygonSamples);
  Boolean(env,out,"basisMatrixVerified",status.basisMatrixVerified);Boolean(env,out,"matrixUniformVerified",status.matrixUniformVerified);
  Property(env,out,"basisMatrixSource",String(env,status.basisMatrixSource));
  Number(env,out,"modelViewDepth",status.modelViewDepth);Number(env,out,"projectionDepth",status.projectionDepth);
  Number(env,out,"matrixUniformMaxError",status.matrixUniformMaxError);
  const char* names[]={"modelViewMatrix","mvpMatrix","normalMatrix"};
  const olex2::harmony::Matrix4* matrices[]={&status.modelViewMatrix,&status.mvpMatrix,&status.normalMatrix};
  for(unsigned j=0;j<3;++j) {
    napi_value matrix;napi_create_array_with_length(env,16,&matrix);
    for(unsigned i=0;i<16;++i) {napi_value value;napi_create_double(env,(*matrices[j])[i],&value);napi_set_element(env,matrix,i,value);}
    Property(env,out,names[j],matrix);
  }
  Property(env,out,"submissionSource",String(env,status.submissionSource));
  Number(env,out,"materialPreset",status.materialPreset); Number(env,out,"materialRevision",status.materialRevision);
  Number(env,out,"materialFlags",status.materialFlags); Property(env,out,"materialSource",String(env,status.materialSource));
  Property(env,out,"materialCanonical",String(env,status.materialCanonical));
  Number(env,out,"ambientR",status.ambientR); Number(env,out,"ambientG",status.ambientG); Number(env,out,"ambientB",status.ambientB);
  Number(env,out,"diffuseR",status.diffuseR); Number(env,out,"diffuseG",status.diffuseG); Number(env,out,"diffuseB",status.diffuseB);
  Number(env,out,"emissionR",status.emissionR); Number(env,out,"emissionG",status.emissionG); Number(env,out,"emissionB",status.emissionB);
  Number(env,out,"specularR",status.specularR);Number(env,out,"specularG",status.specularG);Number(env,out,"specularB",status.specularB);Number(env,out,"shininess",status.shininess);
  Number(env,out,"specularComparedPixels",status.specularComparedPixels);Number(env,out,"specularChangedPixels",status.specularChangedPixels);
  Number(env,out,"specularMaxError",status.specularMaxError);Number(env,out,"specularMaxDelta",status.specularMaxDelta);
  Boolean(env,out,"specularReferenceVerified",status.specularReferenceVerified);
  napi_value samples;napi_create_array_with_length(env,status.specularSamples.size(),&samples);
  for(size_t i=0;i<status.specularSamples.size();++i) {
    const auto& pixel=status.specularSamples[i];napi_value tuple;napi_create_array_with_length(env,9,&tuple);
    const double values[]={double(pixel.x),pixel.diffuse,pixel.specular,double(pixel.actual[0]),double(pixel.actual[1]),double(pixel.actual[2]),double(pixel.baseline[0]),double(pixel.baseline[1]),double(pixel.baseline[2])};
    for(unsigned j=0;j<9;++j){napi_value number;napi_create_double(env,values[j],&number);napi_set_element(env,tuple,j,number);}napi_set_element(env,samples,i,tuple);
  }
  Property(env,out,"specularSamples",samples);
  Number(env,out,"rowMeanScale",status.rowMeanScale); Number(env,out,"rowMinScale",status.rowMinScale); Number(env,out,"rowMaxScale",status.rowMaxScale);
  Boolean(env,out,"materialVerified",status.materialVerified);
  Number(env,out,"materialCommandCount",status.materialCommandCount);
  Property(env,out,"materialCommandsSource",String(env,status.materialCommandsSource));
  Number(env,out,"colorPreset",status.colorPreset); Number(env,out,"colorRevision",status.colorRevision);
  Number(env,out,"colorPacked",status.colorPacked); Property(env,out,"colorSource",String(env,status.colorSource));
  Number(env,out,"colorR",status.colorR); Number(env,out,"colorG",status.colorG); Number(env,out,"colorB",status.colorB); Number(env,out,"colorA",status.colorA);
  Number(env,out,"readbackR",status.readbackR); Number(env,out,"readbackG",status.readbackG); Number(env,out,"readbackB",status.readbackB); Number(env,out,"readbackA",status.readbackA);
  Boolean(env,out,"colorVerified",status.colorVerified);
  Boolean(env,out,"wireframe",status.wireframe);
  Property(env,out,"meshKind",String(env,status.meshKind));
  Number(env,out,"sourceBatches",status.sourceBatches); Number(env,out,"discardedTailVertices",status.discardedTailVertices);
  Number(env,out,"degenerateTrianglesSkipped",status.degenerateTrianglesSkipped); Number(env,out,"geometryColorSamples",status.geometryColorSamples);
  Number(env,out,"meshGeneration",status.meshGeneration); Number(env,out,"meshRevision",status.meshRevision);
  Number(env,out,"meshVertices",status.meshVertices); Number(env,out,"meshTriangles",status.meshTriangles); Number(env,out,"meshIndices",status.meshIndices);
  Property(env,out,"meshSource",String(env,status.meshSource));
  Number(env,out,"width",status.width); Number(env,out,"height",status.height); Number(env,out,"frames",status.frames);
  Number(env,out,"yaw",status.yaw); Number(env,out,"pitch",status.pitch); Number(env,out,"distance",status.distance);
  Property(env,out,"version",String(env,status.version)); Property(env,out,"renderer",String(env,status.renderer));
  Property(env,out,"error",String(env,status.error)); return out;
}
napi_value SetView(napi_env env,napi_callback_info info) {
  size_t count=3; napi_value args[3]; napi_get_cb_info(env,info,&count,args,nullptr,nullptr);
  double values[3]{};
  if (count!=3) { napi_throw_type_error(env,nullptr,"setView requires yaw, pitch and distance"); return nullptr; }
  for (size_t i=0;i<3;++i) if (napi_get_value_double(env,args[i],&values[i])!=napi_ok || !std::isfinite(values[i])) {
    napi_throw_type_error(env,nullptr,"View values must be finite numbers"); return nullptr;
  }
  Renderer::Instance().SetView(values[0],values[1],values[2]); return Graphics(env,nullptr);
}
napi_value SetMesh(napi_env env,napi_callback_info info) {
  size_t count=2; napi_value args[2]; napi_get_cb_info(env,info,&count,args,nullptr,nullptr);
  double generation=0; bool wireframe=false;
  if (count!=2 || napi_get_value_double(env,args[0],&generation)!=napi_ok ||
      !std::isfinite(generation) || generation<0 || generation>5 || std::floor(generation)!=generation ||
      napi_get_value_bool(env,args[1],&wireframe)!=napi_ok) {
    napi_throw_type_error(env,nullptr,"setMesh requires an integer generation 0..5 and a boolean wireframe value"); return nullptr;
  }
  Renderer::Instance().SetMesh(static_cast<unsigned>(generation),wireframe); return Graphics(env,nullptr);
}
napi_value SetPrimitive(napi_env env,napi_callback_info info) {
  size_t count=1; napi_value args[1]; napi_get_cb_info(env,info,&count,args,nullptr,nullptr);
  double kind=0;
  if(count!=1 || napi_get_value_double(env,args[0],&kind)!=napi_ok || !std::isfinite(kind) || kind<0 || kind>3 || std::floor(kind)!=kind) {
    napi_throw_type_error(env,nullptr,"setPrimitive requires an integer kind 0..3"); return nullptr;
  }
  Renderer::Instance().SetPrimitive(static_cast<olex2::harmony::MeshKind>(static_cast<int>(kind))); return Graphics(env,nullptr);
}
napi_value SetColorPreset(napi_env env,napi_callback_info info) {
  size_t count=1; napi_value args[1]; napi_get_cb_info(env,info,&count,args,nullptr,nullptr);
  double preset=0;
  if(count!=1 || napi_get_value_double(env,args[0],&preset)!=napi_ok || !std::isfinite(preset) || preset<0 || preset>2 || std::floor(preset)!=preset) {
    napi_throw_type_error(env,nullptr,"setColorPreset requires an integer preset 0..2"); return nullptr;
  }
  if(!Renderer::Instance().SetColorPreset(static_cast<unsigned>(preset))) {
    napi_throw_error(env,nullptr,"Opaque color conversion failed"); return nullptr;
  }
  return Graphics(env,nullptr);
}
napi_value SetMaterialPreset(napi_env env,napi_callback_info info) {
  size_t count=1; napi_value args[1]; napi_get_cb_info(env,info,&count,args,nullptr,nullptr);
  double preset=0;
  if(count!=1 || napi_get_value_double(env,args[0],&preset)!=napi_ok || !std::isfinite(preset) || preset<0 || preset>3 || std::floor(preset)!=preset) {
    napi_throw_type_error(env,nullptr,"setMaterialPreset requires an integer preset 0..3"); return nullptr;
  }
  if(!Renderer::Instance().SetMaterialPreset(static_cast<unsigned>(preset))) {
    napi_throw_error(env,nullptr,"Opaque material conversion failed"); return nullptr;
  }
  return Graphics(env,nullptr);
}
napi_value Utility(napi_env env,napi_callback_info info) {
  size_t count=1; napi_value args[1]; napi_get_cb_info(env,info,&count,args,nullptr,nullptr);
  size_t length=0;
  if (count!=1 || napi_get_value_string_utf8(env,args[0],nullptr,0,&length)!=napi_ok || length==0 || length>8192) {
    napi_throw_type_error(env,nullptr,"runUtilityChecks requires an application-owned writable directory"); return nullptr;
  }
  std::vector<char> buffer(length+1); napi_get_value_string_utf8(env,args[0],buffer.data(),buffer.size(),&length);
  const auto report=olex2::harmony::RunUtilityChecks(std::string(buffer.data(),length));
  napi_value out,list; napi_create_object(env,&out); napi_create_array_with_length(env,report.checks.size(),&list);
  for (size_t i=0;i<report.checks.size();++i) {
    const auto& check=report.checks[i]; napi_value item; napi_create_object(env,&item);
    Property(env,item,"name",String(env,check.name)); Boolean(env,item,"passed",check.passed); Property(env,item,"detail",String(env,check.detail));
    napi_set_element(env,list,i,item);
  }
  Boolean(env,out,"passed",report.passed); Property(env,out,"checks",list); Property(env,out,"error",String(env,report.error));
  OH_LOG_Print(LOG_APP,LOG_INFO,DOMAIN,"Olex2Native","OLEX2_APP_CHECKS passed=%{public}d count=%{public}zu error=%{public}s",
    report.passed,report.checks.size(),report.error.c_str()); return out;
}
napi_value ExecuteDesktopCommand(napi_env env,napi_callback_info info) {
  napi_value args[1];size_t count=1,length=0;napi_get_cb_info(env,info,&count,args,nullptr,nullptr);
  if(count!=1 || napi_get_value_string_utf8(env,args[0],nullptr,0,&length)!=napi_ok || length==0 || length>2048) {
    napi_throw_type_error(env,nullptr,"executeDesktopCommand requires 1..2048 UTF-8 bytes");return nullptr;
  }
  std::vector<char> text(length+1);napi_get_value_string_utf8(env,args[0],text.data(),text.size(),&length);
  auto before=Renderer::Instance().Status();olex2::harmony::DesktopCommandState initial;
  const std::string kinds[]={"sphere","torus","disks","rims"};
  for(unsigned i=0;i<4;++i)if(before.meshKind==kinds[i])initial.primitive=i;
  initial.generation=before.meshGeneration;initial.wireframe=before.wireframe;initial.colorPreset=before.colorPreset;initial.materialPreset=before.materialPreset;
  initial.yaw=before.yaw;initial.pitch=before.pitch;initial.distance=before.distance;
  auto result=olex2::harmony::ExecuteDesktopCommand(std::string(text.data(),length),initial);bool applied=false;
  if(result.passed && result.changed) {
    applied=Renderer::Instance().ApplyDesktopCommandState(result.state,result.error);
    if(!applied)result.passed=false;
  }
  const auto after=Renderer::Instance().Status();
  if(applied && (!after.initialized || !after.pixelVerified || !after.geometryVerified || !after.error.empty())) {result.passed=false;result.error=after.error.empty()?"Command applied, but actual window validation failed":after.error;}
  napi_value out;napi_create_object(env,&out);
  Boolean(env,out,"passed",result.passed);Boolean(env,out,"changed",result.changed);Boolean(env,out,"applied",applied);Number(env,out,"dispatched",result.dispatched);
  Property(env,out,"text",String(env,result.text));Property(env,out,"error",String(env,result.error));Property(env,out,"source",String(env,olex2::harmony::DesktopCommandSource()));
  // Existing public getter records actual GL uniform/pixel state, not the CPU
  // command's intended output. Do not substitute result.state for this value.
  Property(env,out,"graphics",Graphics(env,nullptr));return out;
}
napi_value ProbeOriginalRenderer(napi_env env,napi_callback_info){auto& owner=Runtime();std::lock_guard<std::mutex> lock(owner.mutex);return String(env,Renderer::Instance().ProbeOriginalRenderer(owner.runtime.get(),owner.filesDir));}
void Created(OH_NativeXComponent* component,void* window) { Renderer::Instance().Attach(component,window); }
void Changed(OH_NativeXComponent* component,void* window) { Renderer::Instance().Resize(component,window); }
void Destroyed(OH_NativeXComponent*,void*) { Renderer::Instance().Destroy(); }
void Touch(OH_NativeXComponent*,void*) {}
OH_NativeXComponent_Callback callbacks={Created,Changed,Destroyed,Touch};
napi_value Init(napi_env env,napi_value exports) {
  napi_property_descriptor descriptors[]={
    {"runOriginalRendererProbe",nullptr,ProbeOriginalRenderer,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"initializeApplicationRuntime",nullptr,InitializeApplicationRuntime,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"getApplicationRuntimeStatus",nullptr,GetApplicationRuntime,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"shutdownApplicationRuntime",nullptr,ShutdownApplicationRuntime,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"evaluateApplicationFunction",nullptr,EvaluateApplicationFunction,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"getOriginalLogs",nullptr,GetOriginalLogs,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"clearOriginalLogs",nullptr,ClearOriginalLogs,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"runCoreCatalog",nullptr,RunCoreCatalog,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"exportApplicationOptions",nullptr,ExportApplicationOptions,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"importApplicationOptions",nullptr,ImportApplicationOptions,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"executeDesktopCommand",nullptr,ExecuteDesktopCommand,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"getGraphicsStatus",nullptr,Graphics,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"setView",nullptr,SetView,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"setMaterialPreset",nullptr,SetMaterialPreset,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"setColorPreset",nullptr,SetColorPreset,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"setPrimitive",nullptr,SetPrimitive,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"setMesh",nullptr,SetMesh,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"runUtilityChecks",nullptr,Utility,nullptr,nullptr,nullptr,napi_default,nullptr}};
  napi_define_properties(env,exports,sizeof(descriptors)/sizeof(descriptors[0]),descriptors);
  bool exists=false; napi_has_named_property(env,exports,OH_NATIVE_XCOMPONENT_OBJ,&exists);
  if (exists) {
    napi_value component_value; OH_NativeXComponent* component=nullptr;
    if (napi_get_named_property(env,exports,OH_NATIVE_XCOMPONENT_OBJ,&component_value)==napi_ok &&
        napi_unwrap(env,component_value,reinterpret_cast<void**>(&component))==napi_ok && component) {
      int result=OH_NativeXComponent_RegisterCallback(component,&callbacks);
      OH_LOG_Print(LOG_APP,LOG_INFO,DOMAIN,"Olex2Native","OLEX2_XCOMPONENT_REGISTER result=%{public}d",result);
    }
  }
  return exports;
}
napi_module module={1,0,nullptr,Init,"olex2native",nullptr,{0}};
}
extern "C" __attribute__((constructor)) void RegisterOlex2NativeModule() { napi_module_register(&module); }
