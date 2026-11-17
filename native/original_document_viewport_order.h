#pragma once
// Platform-only canonical mapping: complete immutable descriptors, live object
// pointers retained only as targets, and stable byte identities as the order.
#include "original_document_viewport.h"
#include <algorithm>
#include <unordered_set>
#include <utility>

namespace olex2::harmony::viewport_objects {
template<class Object> struct ViewportObjectRecord {
  Object* object;
  std::string identity,descriptor;
  bool selected;
};
template<class Object> struct CanonicalViewportOrder {
  std::vector<Object*> objects;
  std::vector<size_t> selected;
  std::string bytes;
};
inline bool ViewportIdentityLess(const std::string& left,const std::string& right){
  return std::lexicographical_compare(left.begin(),left.end(),right.begin(),right.end(),
    [](char a,char b){return static_cast<unsigned char>(a)<static_cast<unsigned char>(b);});
}
template<class Object>
CanonicalViewportOrder<Object> CanonicaliseViewportInventory(std::vector<ViewportObjectRecord<Object>> records){
  ViewportRequire(records.size()<=MaximumViewportObjects,"Original inventory exceeds canonical object bound; nothing truncated");
  // Pointer membership is checked, never used to order or identify objects.
  std::unordered_set<const Object*> seen;
  for(const auto& record:records)
    ViewportRequire(record.object&&seen.insert(record.object).second,"Null/duplicate original canonical object target");
  std::sort(records.begin(),records.end(),[](const auto& left,const auto& right){
    return ViewportIdentityLess(left.identity,right.identity);
  });
  ViewportInventoryEncoding encoding;CanonicalViewportOrder<Object> result;
  result.objects.reserve(records.size());
  for(const auto& record:records){
    // The encoder rejects empty/ambiguous identities and retains every byte
    // of every descriptor. Selection remains a separate exact index mapping.
    encoding.Add(record.identity,record.descriptor);
    if(record.selected){
      ViewportRequire(result.selected.size()<MaximumViewportSelection,"Canonical original selection exceeds bound; nothing truncated");
      result.selected.push_back(result.objects.size());
    }
    result.objects.push_back(record.object);
  }
  result.bytes=encoding.Bytes();return result;
}
}
