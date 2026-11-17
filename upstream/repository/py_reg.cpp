/******************************************************************************
* Copyright (c) 2004-2025 O. Dolomanov, OlexSys                               *
*                                                                             *
* This file is part of the OlexSys Development Framework.                     *
*                                                                             *
* This source file is distributed under the terms of the licence located in   *
* the root folder.                                                            *
******************************************************************************/

#if defined(__OHOS__) && defined(_PYTHON)
#include <Python.h>
#include <bundle/native_interface_bundle.h>
#include <database/preferences/oh_preferences.h>
#include <database/preferences/oh_preferences_err_code.h>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
struct UnsupportedHive : std::runtime_error { using std::runtime_error::runtime_error; };
struct RootInfo { const char* file; };
RootInfo SelectRoot(PyObject* value) {
  if (!value || value == Py_None) return {"olex2_registry"};
  const char* raw = PyUnicode_AsUTF8(value);
  if (!raw) throw std::runtime_error("Registry root must be Unicode");
  std::string name(raw);
  std::transform(name.begin(), name.end(), name.begin(),
    [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  if (name.empty() || name == "HKCU" || name == "HKEY_CURRENT_USER") return {"olex2_registry"};
  throw UnsupportedHive("Harmony only provides an app-private current-user registry store; system and machine hives are unavailable");
}
std::string Utf8(PyObject* value, const char* label) {
  if (!PyUnicode_Check(value)) throw std::runtime_error(std::string(label)+" must be Unicode");
  Py_ssize_t length=0;const char* text=PyUnicode_AsUTF8AndSize(value,&length);
  if (!text) throw std::runtime_error(std::string("Could not encode ")+label+" as UTF-8");
  return std::string(text,static_cast<size_t>(length));
}
std::string CanonicalPath(const std::string& value) {
  if (value.size()>2048) throw std::runtime_error("Registry path exceeds 2048 UTF-8 bytes");
  std::string out;out.reserve(value.size());bool separator=true;
  for (unsigned char c:value) {
    if (c=='/' || c=='\\') { if(!separator)out.push_back('/');separator=true;continue; }
    out.push_back(c<0x80?static_cast<char>(std::tolower(c)):static_cast<char>(c));separator=false;
  }
  while(!out.empty()&&out.back()=='/')out.pop_back();
  return out;
}
std::string EncodeComponent(const std::string& value) {
  static const char hex[]="0123456789ABCDEF";std::string out;out.reserve(value.size());
  for(unsigned char c:value) {
    if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.')out.push_back(static_cast<char>(c));
    else {out.push_back('%');out.push_back(hex[c>>4]);out.push_back(hex[c&15]);}
  }
  return out;
}
int Hex(char c) { if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return -1; }
std::string DecodeComponent(const std::string& value) {
  std::string out;out.reserve(value.size());
  for(size_t i=0;i<value.size();++i) {
    if(value[i]=='%') {
      if(i+2>=value.size()||Hex(value[i+1])<0||Hex(value[i+2])<0)throw std::runtime_error("Stored registry value name is malformed");
      out.push_back(static_cast<char>((Hex(value[i+1])<<4)|Hex(value[i+2])));i+=2;
    } else out.push_back(value[i]);
  }
  return out;
}
[[noreturn]] void PreferencesError(const char* operation,int code) {
  throw std::runtime_error(std::string(operation)+" failed (Preferences status "+std::to_string(code)+")");
}
void Check(int code,const char* operation) { if(code!=PREFERENCES_OK)PreferencesError(operation,code); }
class PreferenceStore {
  OH_PreferencesOption* option_=nullptr;
  OH_Preferences* store_=nullptr;
public:
  explicit PreferenceStore(const RootInfo& root) {
    option_=OH_PreferencesOption_Create();if(!option_)throw std::runtime_error("Could not allocate Preferences options");
    try {
      Check(OH_PreferencesOption_SetFileName(option_,root.file),"Select app-private registry store");
      OH_NativeBundle_ApplicationInfo info=OH_NativeBundle_GetCurrentApplicationInfo();
      std::unique_ptr<char,decltype(&std::free)> bundleName(info.bundleName,&std::free);
      std::unique_ptr<char,decltype(&std::free)> fingerprint(info.fingerprint,&std::free);
      if(!bundleName||!*bundleName)throw std::runtime_error("Could not resolve the current Harmony bundle name");
      const std::string bundle(bundleName.get());
      Check(OH_PreferencesOption_SetBundleName(option_,bundle.c_str()),"Select current application bundle");
      int error=PREFERENCES_OK;store_=OH_Preferences_Open(option_,&error);
      if(!store_)PreferencesError("Open app-private registry store",error);
      OH_PreferencesOption* openedOption=option_;option_=nullptr;
      Check(OH_PreferencesOption_Destroy(openedOption),"Release Preferences options");
    } catch (...) {
      if(option_){OH_PreferencesOption_Destroy(option_);option_=nullptr;}
      if(store_){OH_Preferences_Close(store_);store_=nullptr;}
      throw;
    }
  }
  ~PreferenceStore(){if(store_)OH_Preferences_Close(store_);if(option_)OH_PreferencesOption_Destroy(option_);}
  PreferenceStore(const PreferenceStore&)=delete;PreferenceStore& operator=(const PreferenceStore&)=delete;
  OH_Preferences* get()const{return store_;}
  void Flush(){Check(OH_Preferences_Flush(store_),"Persist app-private registry store");}
};
struct PairList {
  OH_PreferencesPair* pairs=nullptr;uint32_t count=0;
  explicit PairList(OH_Preferences* store) {
    const int status=OH_Preferences_GetAll(store,&pairs,&count);
    if(status==PREFERENCES_ERROR_KEY_NOT_FOUND){
      if(pairs)OH_PreferencesPair_Destroy(pairs,count);
      pairs=nullptr;count=0;return;
    }
    if(status!=PREFERENCES_OK){
      if(pairs)OH_PreferencesPair_Destroy(pairs,count);
      pairs=nullptr;count=0;
      PreferencesError("Enumerate app-private registry store",status);
    }
    if(count>65536){OH_PreferencesPair_Destroy(pairs,count);pairs=nullptr;count=0;throw std::runtime_error("Registry store exceeds the 65536-entry safety limit");}
  }
  ~PairList(){if(pairs)OH_PreferencesPair_Destroy(pairs,count);}
  PairList(const PairList&)=delete;PairList& operator=(const PairList&)=delete;
};
std::string KeyMarker(const std::string& path){return "key/"+path;}
std::string ValuePrefix(const std::string& path){return "value/"+path+"/";}
std::string ValueKey(const std::string& path,const std::string& name){return ValuePrefix(path)+EncodeComponent(name);}
void SetKey(OH_Preferences* store,const std::string& path){Check(OH_Preferences_SetBool(store,KeyMarker(path).c_str(),true),"Create registry key");}
bool ContainsKey(OH_Preferences* store,const std::string& path) {
  if(path.empty())return true;
  PairList list(store);const std::string key=KeyMarker(path),sub=key+"/",values=ValuePrefix(path);
  for(uint32_t i=0;i<list.count;++i){const char* name=OH_PreferencesPair_GetKey(list.pairs,i);if(!name)continue;const std::string item(name);
    if(item==key||item.compare(0,sub.size(),sub)==0||item.compare(0,values.size(),values)==0)return true;}
  return false;
}
std::string StoredValueToText(const OH_PreferencesValue* value) {
  if(!value)return {};
  switch(OH_PreferencesValue_GetValueType(value)) {
    case PREFERENCE_TYPE_STRING:{
      char* raw=nullptr;uint32_t length=0;
      const int status=OH_PreferencesValue_GetString(value,&raw,&length);
      std::unique_ptr<char,decltype(&OH_Preferences_FreeString)> text(raw,&OH_Preferences_FreeString);
      Check(status,"Read string preference");
      return std::string(text?text.get():"",text?length:0);
    }
    case PREFERENCE_TYPE_INT:{int n=0;Check(OH_PreferencesValue_GetInt(value,&n),"Read integer preference");return std::to_string(n);}
    case PREFERENCE_TYPE_BOOL:{bool b=false;Check(OH_PreferencesValue_GetBool(value,&b),"Read boolean preference");return b?"true":"false";}
    case PREFERENCE_TYPE_INT64:{int64_t n=0;Check(OH_PreferencesValue_GetInt64(value,&n),"Read 64-bit integer preference");return std::to_string(n);}
    case PREFERENCE_TYPE_DOUBLE:{double n=0;Check(OH_PreferencesValue_GetDouble(value,&n),"Read floating-point preference");std::ostringstream out;out<<std::setprecision(17)<<n;return out.str();}
    default:return {};
  }
}
PyObject* PythonString(const std::string& value){return PyUnicode_DecodeUTF8(value.data(),static_cast<Py_ssize_t>(value.size()),"strict");}
void RejectNul(const std::string& value,const char* label){
  if(value.find('\0')!=std::string::npos)throw std::runtime_error(std::string(label)+" cannot contain a NUL character");
}
PyObject* Invoke(PyObject* (*fn)(PyObject*,PyObject*),PyObject* self,PyObject* args) {
  try{return fn(self,args);}catch(const UnsupportedHive& e){PyErr_SetString(PyExc_NotImplementedError,e.what());return nullptr;}catch(const std::exception& e){PyErr_SetString(PyExc_RuntimeError,e.what());return nullptr;}catch(...){PyErr_SetString(PyExc_RuntimeError,"Unknown native exception in olex_reg");return nullptr;}
}
#define OLEX_REG_WRAPPER(name) PyObject* name##_safe(PyObject* self,PyObject* args){return Invoke(&name,self,args);}
PyObject* Exists(PyObject*,PyObject* args){PyObject *keyArg=nullptr,*rootArg=nullptr;if(!PyArg_ParseTuple(args,"U|U",&keyArg,&rootArg))return nullptr;const auto root=SelectRoot(rootArg);const auto rawPath=Utf8(keyArg,"registry key");RejectNul(rawPath,"Registry key");const auto path=CanonicalPath(rawPath);PreferenceStore store(root);return PyBool_FromLong(ContainsKey(store.get(),path));}
PyObject* ListKeys(PyObject*,PyObject* args){PyObject *keyArg=nullptr,*rootArg=nullptr;if(!PyArg_ParseTuple(args,"U|U",&keyArg,&rootArg))return nullptr;const auto root=SelectRoot(rootArg);const auto rawPath=Utf8(keyArg,"registry key");RejectNul(rawPath,"Registry key");const auto path=CanonicalPath(rawPath);PreferenceStore store(root);if(!ContainsKey(store.get(),path))Py_RETURN_NONE;PairList list(store.get());const std::string prefix=KeyMarker(path)+(path.empty()?"":"/");std::unordered_set<std::string> names;for(uint32_t i=0;i<list.count;++i){const char* raw=OH_PreferencesPair_GetKey(list.pairs,i);if(!raw)continue;std::string item(raw);if(item.compare(0,prefix.size(),prefix)!=0)continue;const auto remainder=item.substr(prefix.size());const auto slash=remainder.find('/');if(!remainder.empty())names.insert(remainder.substr(0,slash));}std::vector<std::string> ordered(names.begin(),names.end());std::sort(ordered.begin(),ordered.end());PyObject* result=PyList_New(static_cast<Py_ssize_t>(ordered.size()));if(!result)return nullptr;for(size_t i=0;i<ordered.size();++i){PyObject* item=PythonString(ordered[i]);if(!item){Py_DECREF(result);return nullptr;}PyList_SET_ITEM(result,static_cast<Py_ssize_t>(i),item);}return result;}
PyObject* ListValues(PyObject*,PyObject* args){PyObject *keyArg=nullptr,*rootArg=nullptr;if(!PyArg_ParseTuple(args,"U|U",&keyArg,&rootArg))return nullptr;const auto root=SelectRoot(rootArg);const auto rawPath=Utf8(keyArg,"registry key");RejectNul(rawPath,"Registry key");const auto path=CanonicalPath(rawPath);PreferenceStore store(root);if(!ContainsKey(store.get(),path))Py_RETURN_NONE;PairList list(store.get());const std::string prefix=ValuePrefix(path);std::unordered_set<std::string> names;for(uint32_t i=0;i<list.count;++i){const char* raw=OH_PreferencesPair_GetKey(list.pairs,i);if(!raw)continue;std::string item(raw);if(item.compare(0,prefix.size(),prefix)!=0)continue;const auto remainder=item.substr(prefix.size());if(remainder.find('/')!=std::string::npos)continue;names.insert(DecodeComponent(remainder));}std::vector<std::string> ordered(names.begin(),names.end());std::sort(ordered.begin(),ordered.end());PyObject* result=PyList_New(static_cast<Py_ssize_t>(ordered.size()));if(!result)return nullptr;for(size_t i=0;i<ordered.size();++i){PyObject* item=PythonString(ordered[i]);if(!item){Py_DECREF(result);return nullptr;}PyList_SET_ITEM(result,static_cast<Py_ssize_t>(i),item);}return result;}
PyObject* Read(PyObject*,PyObject* args){PyObject *keyArg=nullptr,*valueArg=nullptr,*rootArg=nullptr;if(!PyArg_ParseTuple(args,"UU|U",&keyArg,&valueArg,&rootArg))return nullptr;const auto root=SelectRoot(rootArg);const auto rawPath=Utf8(keyArg,"registry key");RejectNul(rawPath,"Registry key");const auto path=CanonicalPath(rawPath);const auto name=Utf8(valueArg,"registry value name");RejectNul(name,"Registry value name");PreferenceStore store(root);OH_PreferencesValue* value=nullptr;const int status=OH_Preferences_GetValue(store.get(),ValueKey(path,name).c_str(),&value);std::unique_ptr<OH_PreferencesValue,void(*)(OH_PreferencesValue*)> owner(value,OH_PreferencesValue_Destroy);if(status==PREFERENCES_ERROR_KEY_NOT_FOUND)Py_RETURN_NONE;Check(status,"Read registry value");return PythonString(StoredValueToText(value));}
PyObject* CreateKey(PyObject*,PyObject* args){PyObject *keyArg=nullptr,*rootArg=nullptr;if(!PyArg_ParseTuple(args,"U|U",&keyArg,&rootArg))return nullptr;const auto root=SelectRoot(rootArg);const auto rawPath=Utf8(keyArg,"registry key");RejectNul(rawPath,"Registry key");const auto path=CanonicalPath(rawPath);PreferenceStore store(root);SetKey(store.get(),path);store.Flush();Py_RETURN_TRUE;}
PyObject* Write(PyObject*,PyObject* args){PyObject *keyArg=nullptr,*valueArg=nullptr,*dataArg=nullptr,*rootArg=nullptr;if(!PyArg_ParseTuple(args,"UUU|U",&keyArg,&valueArg,&dataArg,&rootArg))return nullptr;const auto root=SelectRoot(rootArg);const auto rawPath=Utf8(keyArg,"registry key");RejectNul(rawPath,"Registry key");const auto path=CanonicalPath(rawPath);const auto name=Utf8(valueArg,"registry value name");RejectNul(name,"Registry value name");const auto data=Utf8(dataArg,"registry value");RejectNul(data,"Registry value");PreferenceStore store(root);SetKey(store.get(),path);Check(OH_Preferences_SetString(store.get(),ValueKey(path,name).c_str(),data.c_str()),"Write registry value");store.Flush();Py_RETURN_TRUE;}
PyObject* Delete(PyObject*,PyObject* args){PyObject *keyArg=nullptr,*valueArg=nullptr,*rootArg=nullptr;if(!PyArg_ParseTuple(args,"UU|U",&keyArg,&valueArg,&rootArg))return nullptr;const auto root=SelectRoot(rootArg);const auto rawPath=Utf8(keyArg,"registry key");RejectNul(rawPath,"Registry key");const auto path=CanonicalPath(rawPath);const auto name=Utf8(valueArg,"registry value name");RejectNul(name,"Registry value name");PreferenceStore store(root);const auto encoded=ValueKey(path,name);OH_PreferencesValue* value=nullptr;const int exists=OH_Preferences_GetValue(store.get(),encoded.c_str(),&value);std::unique_ptr<OH_PreferencesValue,void(*)(OH_PreferencesValue*)> owner(value,OH_PreferencesValue_Destroy);if(exists==PREFERENCES_ERROR_KEY_NOT_FOUND)Py_RETURN_FALSE;Check(exists,"Check registry value before deletion");owner.reset();Check(OH_Preferences_Delete(store.get(),encoded.c_str()),"Delete registry value");store.Flush();Py_RETURN_TRUE;}
OLEX_REG_WRAPPER(Exists) OLEX_REG_WRAPPER(ListKeys) OLEX_REG_WRAPPER(ListValues) OLEX_REG_WRAPPER(Read) OLEX_REG_WRAPPER(CreateKey) OLEX_REG_WRAPPER(Write) OLEX_REG_WRAPPER(Delete)
#undef OLEX_REG_WRAPPER
PyMethodDef Methods[]={
  {"Exists",Exists_safe,METH_VARARGS,"Check a key in the app-private Harmony registry compatibility store."},
  {"ListKeys",ListKeys_safe,METH_VARARGS,"List direct child keys in the app-private Harmony registry compatibility store."},
  {"ListValues",ListValues_safe,METH_VARARGS,"List direct values in the app-private Harmony registry compatibility store."},
  {"Read",Read_safe,METH_VARARGS,"Read a string, integer, or boolean value as text."},
  {"CreateKey",CreateKey_safe,METH_VARARGS,"Create a key in the app-private Harmony registry compatibility store."},
  {"Write",Write_safe,METH_VARARGS,"Write a string value in the app-private Harmony registry compatibility store."},
  {"Delete",Delete_safe,METH_VARARGS,"Delete a value from the app-private Harmony registry compatibility store."},
  {nullptr,nullptr,0,nullptr}
};
PyModuleDef Module={PyModuleDef_HEAD_INIT,"olex_reg","Harmony app-private registry compatibility backed by Preferences.",-1,Methods,nullptr,nullptr,nullptr,nullptr};
}
extern "C" PyObject* PyInit_olex_reg(void){return PyModule_Create(&Module);}

#elif defined(_PYTHON) && defined(__WXWIDGETS__) && defined(_WIN32)
#include "py_reg.h"
#include <wx/msw/registry.h>

PyMethodDef py_reg::Methods[] = {
  {"Exists", py_reg::Exists, METH_VARARGS, "checks if specified key exists"},
  {"ListKeys", py_reg::ListKeys, METH_VARARGS, "lists specified key sub-keys"},
  {"ListValues", py_reg::ListValues, METH_VARARGS, "lists specified key values"},
  {"Read", py_reg::Read, METH_VARARGS, "reads specified key value"},
  {NULL, NULL, 0, NULL}
};

olxcstr& py_reg::ModuleName() {
  static olxcstr mn = "olex_reg";
  return mn;
}

PyObject* py_reg::PyInit() {
  return PythonExt::init_module(ModuleName(), Methods);
}
//..................................................................................................
int RootNameToConst(const olxstr& n, int def = wxRegKey::HKCU) {
  if (n.Equalsi("HKCR")) {
    return wxRegKey::HKCR;
  }
  if (n.Equalsi("HKCU")) {
    return wxRegKey::HKCU;
  }
  if (n.Equalsi("HKLM")) {
    return wxRegKey::HKLM;
  }
  if (n.Equalsi("HKCC")) {
    return wxRegKey::HKCC;
  }
  if (n.Equalsi("HKU")) {
    return wxRegKey::HKUSR;
  }
  return def;
}

//..................................................................................................
PyObject* py_reg::Exists(PyObject* self, PyObject* args) {
  olxstr key_name, root;
  if (!PythonExt::ParseTuple(args, "w|w", &key_name, &root)) {
    return PythonExt::InvalidArgumentException(__OlxSourceInfo, "w|w");
  }
  wxRegKey k((wxRegKey::StdKey)RootNameToConst(root), key_name.u_str());
  return k.Exists() ? PythonExt::PyTrue() : PythonExt::PyFalse();
}
//..................................................................................................
PyObject* py_reg::ListKeys(PyObject* self, PyObject* args) {
  olxstr key_name, root;
  if (!PythonExt::ParseTuple(args, "w|w", &key_name, &root)) {
    return PythonExt::InvalidArgumentException(__OlxSourceInfo, "w|w");
  }
  wxRegKey key((wxRegKey::StdKey)RootNameToConst(root), key_name.u_str());
  if (!key.Exists()) {
    return PythonExt::PyNone();
  }
  wxString skn;
  long index = 0;
  PyObject* rv = PyList_New(0);
  for (bool cont = key.GetFirstKey(skn, index); cont;
    cont = key.GetNextKey(skn, index))
  {
    PyList_Append(rv, PythonExt::BuildString(skn));
  }
  return rv;
}
//..................................................................................................
PyObject* py_reg::ListValues(PyObject* self, PyObject* args) {
  olxstr key_name, root;
  if (!PythonExt::ParseTuple(args, "w|w", &key_name, &root)) {
    return PythonExt::InvalidArgumentException(__OlxSourceInfo, "w|w");
  }
  wxRegKey key((wxRegKey::StdKey)RootNameToConst(root), key_name.u_str());
  if (!key.Exists()) {
    return PythonExt::PyNone();
  }
  wxString svn;
  long index = 0;
  PyObject* rv = PyList_New(0);

  for (bool cont = key.GetFirstValue(svn, index); cont;
    cont = key.GetNextValue(svn, index))
  {
    PyList_Append(rv, PythonExt::BuildString(svn));
  }
  return rv;
}
//..................................................................................................
PyObject* py_reg::Read(PyObject* self, PyObject* args) {
  olxstr key_name, value_name, root;
  if (!PythonExt::ParseTuple(args, "ww|w", &key_name, &value_name, &root)) {
    return PythonExt::InvalidArgumentException(__OlxSourceInfo, "ww|w");
  }
  wxRegKey key((wxRegKey::StdKey)RootNameToConst(root), key_name.u_str());
  if (!key.Exists()) {
    return PythonExt::PyNone();
  }
  wxString v;
  if (key.QueryRawValue(value_name.u_str(), v)) {
    return PythonExt::BuildString(v);
  }
  return PythonExt::PyNone();
}
//..................................................................................................
#endif // _PYTHON && __WXWIDGETS__ && _WIN32