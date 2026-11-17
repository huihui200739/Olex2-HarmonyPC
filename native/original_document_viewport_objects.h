#pragma once
#include "original_document_viewport.h"
#include "original_document_viewport_order.h"
#include "platform_sha256.h"
#include "gxapp.h"
#include "dbasis.h"
#include "dsphere.h"
#include "atomlegend.h"
#include "gllabels.h"
#include "xgrid.h"
#include "glbackground.h"
#include "eutf8.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>

namespace olex2::harmony {
namespace viewport_objects {
inline std::string Utf8(const olxstr& s){const auto bytes=TUtf8::Encode(s);return std::string(bytes.c_str(),bytes.Length());}
// Canonical bytes independent of locale, pointer, Tag, owner/revision or GL IDs.
struct Record {
  std::string bytes;
  void Text(const std::string& s){bytes+=std::to_string(s.size())+":"+s+";";}
  void Integer(uint64_t n){Text(std::to_string(n));}
  void Real(double n){
    ViewportRequire(std::isfinite(n),"Non-finite original object identity");
    if(n==0)n=0;uint64_t bits=0;static_assert(sizeof(bits)==sizeof(n));std::memcpy(&bits,&n,sizeof(n));
    std::string hex(16,'0');for(unsigned i=0;i<16;++i)hex[15-i]="0123456789abcdef"[(bits>>(4*i))&15];Text(hex);
  }
  template<class V> void Vector(const V& v){for(unsigned i=0;i<3;++i)Real(v[i]);}
};
inline std::string AtomIdentity(const TXAtom& atom,TGXApp& app){
  ViewportRequire(typeid(atom)==typeid(TXAtom),"Unsupported original atom subclass");
  const auto& au=app.XFile().GetAsymmUnit();const auto& a=atom.CAtom();
  ViewportRequire(a.GetParent()==&au&&a.GetId()<au.AtomCount()&&&au.GetAtom(a.GetId())==&a,
    "Original display object is not owned by the current source");
  Record r;r.Text("source-atom-v1");r.Integer(a.GetId());r.Text(Utf8(atom.GetLabel()));r.Integer(atom.GetType().z);
  r.Vector(atom.ccrd());r.Vector(atom.crd());
  const auto& m=atom.GetMatrix();r.Integer(m.GetId());for(unsigned i=0;i<3;++i)r.Vector(m.r[i]);r.Vector(m.t);
  return r.bytes;
}
struct Identity {std::string bytes;std::type_index type;};
class IdentityMap {
  std::unordered_map<const AGDrawObject*,Identity> ids_;
public:
  template<class T> void Add(T& object,const std::string& id){
    ViewportRequire(typeid(object)==typeid(T),"Unsupported original singleton/object subclass");
    ViewportRequire(ids_.emplace(&object,Identity{id,typeid(T)}).second,"Original object has multiple incompatible identity roles");
  }
  explicit IdentityMap(TGXApp& app){
    auto& renderer=app.GetRenderer();
    Add(renderer.GetSelection(),"renderer/selection");
    if(renderer.Background())Add(*renderer.Background(),"renderer/background");
    if(renderer.Ceiling())Add(*renderer.Ceiling(),"renderer/ceiling");
    Add(app.DBasis(),"application/basis");Add(app.DSphere(),"application/sphere");
    Add(app.DFrame(),"application/frame");Add(app.GetFader(),"application/fader");
    Add(app.AtomLegend(),"application/legend");Add(app.Get3DFrame(),"application/3d-frame");
    Add(app.GetLabels(),"application/labels");Add(app.XGrid(),"application/grid");
    // These labels are real owned renderer objects even while hidden. Use
    // the pinned owner's public index/pointer, never a type/collection guess.
    auto& basis=app.DBasis();
    ViewportRequire(basis.LabelCount()==3,"Unsupported original basis child-label count");
    for(size_t i=0;i<basis.LabelCount();++i)
      Add(basis.GetLabel(i),"application/basis/label/"+std::to_string(i));
    if(auto* cell=app.XFile().DUnitCell){
      Add(*cell,"source/unit-cell");
      ViewportRequire(cell->LabelCount()==4,"Unsupported original unit-cell child-label count");
      for(size_t i=0;i<cell->LabelCount();++i)
        Add(cell->GetLabel(i),"source/unit-cell/label/"+std::to_string(i));
    }
    auto& frame=app.Get3DFrame();
    ViewportRequire(frame.Faces.Count()==6,"Unsupported original frame child-face count");
    for(size_t i=0;i<frame.Faces.Count();++i){
      ViewportRequire(frame.Faces[i].GetId()==i,"Original frame face identity/index disagrees");
      Add(frame.Faces[i],"application/3d-frame/face/"+std::to_string(i));
    }
    auto atoms=app.GetAtoms();auto bonds=app.GetBonds();
    ViewportRequire(atoms.count<=MaximumViewportObjects&&bonds.count<=MaximumViewportObjects,"Source identity list exceeds bound");
    while(atoms.HasNext()){
      auto& atom=atoms.Next();const auto id=AtomIdentity(atom,app);Add(atom,id);Add(atom.GetGlLabel(),"atom-label/"+id);
    }
    while(bonds.HasNext()){
      auto& bond=bonds.Next();Record r;r.Text("source-bond-v1");r.Text(AtomIdentity(bond.A(),app));r.Text(AtomIdentity(bond.B(),app));
      r.Integer(bond.GetType());r.Integer(bond.GetOrder());Add(bond,r.bytes);Add(bond.GetGlLabel(),"bond-label/"+r.bytes);
    }
  }
  std::string Get(const AGDrawObject& object)const{
    const auto found=ids_.find(&object);
    if(found!=ids_.end()){
      ViewportRequire(found->second.type==std::type_index(typeid(object)),"Original object identity/type changed");return found->second.bytes;
    }
    // The pinned TXGrid factory always creates exactly one private TLegend and
    // one TGlTextBox, but exposes neither pointer. Admit ONLY its two
    // nonselectable reserved roles with exact original RTTI+collection. They
    // are never selection targets; duplicate roles fail the full inventory.
    // No general collection-name/type fallback exists for arbitrary objects.
    const auto collection=Utf8(object.GetCollectionName());
    if(!object.IsSelectable()){
      if(collection=="XGrid_Label"&&typeid(object)==typeid(TGlTextBox))return "grid-private/label";
      if(collection=="XGrid_Legend"&&std::string(typeid(object).name())=="N5gxlib6TXGrid7TLegendE")return "grid-private/legend";
    }
    throw std::runtime_error("Original inventory cannot fully identify object type/collection: "+std::string(typeid(object).name())+"/"+collection);
  }
};
struct Inventory {
  std::vector<AGDrawObject*> objects;
  std::vector<size_t> selected;
  std::string sha256;
};
inline Inventory ReadInventory(TGXApp& app){
  auto& renderer=app.GetRenderer();const size_t count=renderer.ObjectCount();auto& selection=renderer.GetSelection();
  // The pinned TGlRenderer constructor never initializes Picking; only
  // SelectObject installs its scoped value. Do not read that dormant bool on
  // the first normal document. Runtime/Renderer locks serialize the complete
  // picking call with this operation; actual draw-mode checks remain below.
  ViewportRequire(renderer.IsSceneComplete(),"Original viewport scene is incomplete");
  ViewportRequire(!renderer.IsSelecting(),"Original viewport selection drawing is active");
  ViewportRequire(!renderer.IsPerspectiveEnabled(),"Original perspective view is not representable by this viewport schema");
  ViewportRequire(renderer.GetStereoFlag()==0,"Original stereo view is not representable by this viewport schema");
  ViewportRequire(count<=MaximumViewportObjects&&selection.Count()<=MaximumViewportSelection,"Original inventory/selection exceeds bound; nothing truncated");
  ViewportRequire(selection.GetParentGroup()==nullptr,"Nested original selection container cannot be represented");
  std::unordered_set<const AGDrawObject*> members,seen;
  for(size_t i=0;i<selection.Count();++i)ViewportRequire(members.insert(&selection.GetObject(i)).second,"Duplicate original selection membership");
  IdentityMap identities(app);std::vector<ViewportObjectRecord<AGDrawObject>> records;records.reserve(count);
  for(size_t i=0;i<count;++i){
    auto& object=renderer.GetObject(i);
    ViewportRequire(seen.insert(&object).second&&&object.GetParent()==&renderer,"Duplicate/foreign original renderer object");
    const bool container=&object==&selection;
    ViewportRequire(container||!object.IsGroup(),"Original custom/nested groups are not representable by flat viewport indices");
    const bool selected=members.count(&object)!=0;
    ViewportRequire(!container||!selected,"Original selection contains itself");
    if(!container){
      const std::string selectionDetail = std::string("Original GetSelection and IsSelected disagree at index ")+
        std::to_string(i)+"/"+std::to_string(count)+": member="+(selected?"1":"0")+" flag="+(object.IsSelected()?"1":"0")+
        " grouped="+(object.IsGrouped()?"1":"0")+" selectable="+(object.IsSelectable()?"1":"0")+
        " visible="+(object.IsVisible()?"1":"0")+" type="+std::string(typeid(object).name())+
        " collection="+Utf8(object.GetCollectionName())+" selectionCount="+std::to_string(selection.Count());
      ViewportRequire(object.IsSelected()==selected, selectionDetail.c_str());
      const std::string groupingDetail = std::string("Original grouping cannot be represented losslessly at index ")+
        std::to_string(i)+": collection="+Utf8(object.GetCollectionName())+" parentSelection="+
        (object.GetParentGroup()==&selection?"1":"0")+" grouped="+(object.IsGrouped()?"1":"0");
      ViewportRequire(selected?(object.GetParentGroup()==&selection&&object.IsGrouped()):(!object.GetParentGroup()&&!object.IsGrouped()),
        groupingDetail.c_str());
      if(selected)ViewportRequire(object.IsVisible()&&object.IsSelectable(),"Original selected object is not visible/selectable");
    }
    Record descriptor;descriptor.Text(Utf8(object.GetCollectionName()));descriptor.Text(typeid(object).name());
    descriptor.Integer(object.IsVisible());descriptor.Integer(object.IsSelectable());descriptor.Integer(object.IsGroupable());descriptor.Integer(object.IsPrintable());
    descriptor.Integer(object.HasPrimitives());
    if(object.HasPrimitives()){
      const auto& primitives=object.GetPrimitives();descriptor.Integer(primitives.PrimitiveCount());
      for(size_t p=0;p<primitives.PrimitiveCount();++p){const auto& primitive=primitives.GetPrimitive(p);
        descriptor.Text(Utf8(primitive.GetName()));descriptor.Integer(primitive.GetType());descriptor.Integer(primitive.GetProperties().IsIdentityDraw());}
    }
    records.push_back({&object,identities.Get(object),std::move(descriptor.bytes),selected});
  }
  for(const auto* object:members)ViewportRequire(seen.count(object)!=0,"Original selection contains an object absent from the full renderer inventory");
  // Renderer GetObject order is creation order, not a persistent source
  // identity. Canonicalise the FULL inventory and derive selection indices
  // from that same live-object mapping; no descriptor or guard is omitted.
  auto ordered=CanonicaliseViewportInventory(std::move(records));Inventory result;
  result.objects=std::move(ordered.objects);result.selected=std::move(ordered.selected);
  result.sha256=PlatformSha256(ordered.bytes);return result;
}
inline void ValidateSelection(const Inventory& inventory,const std::vector<size_t>& indices,TGlRenderer& renderer){
  bool modeKnown=false,identityDraw=false;
  for(size_t index:indices){
    ViewportRequire(index<inventory.objects.size(),"Viewport selection index exceeds original inventory");
    const auto& object=*inventory.objects[index];
    ViewportRequire(&object!=&renderer.GetSelection()&&!object.IsGroup()&&object.IsVisible()&&object.IsSelectable(),"Viewport target is not an original selectable object");
    ViewportRequire(!object.GetParentGroup()||object.GetParentGroup()==&renderer.GetSelection(),"Viewport target belongs to an unsupported original group");
    // TGlGroup::Add refuses mixed identity-draw modes. Prove compatibility
    // BEFORE clearing anything, rather than discovering a partial selection.
    if(object.HasPrimitives()&&object.GetPrimitives().PrimitiveCount()){
      const bool mode=object.GetPrimitives().GetPrimitive(0).GetProperties().IsIdentityDraw();
      ViewportRequire(!modeKnown||identityDraw==mode,"Viewport selection mixes incompatible original draw modes");modeKnown=true;identityDraw=mode;
    }
  }
}
inline void ApplySelection(TGlRenderer& renderer,const Inventory& inventory,const std::vector<size_t>& indices){
  renderer.SelectAll(false);for(size_t i:indices)renderer.Select(*inventory.objects[i],true);
  ViewportRequire(renderer.GetSelection().Count()==indices.size(),"Original renderer refused part of the requested selection");
  for(size_t i:indices)ViewportRequire(inventory.objects[i]->IsSelected()&&renderer.GetSelection().Contains(*inventory.objects[i]),"Original renderer selection readback differs");
}
inline TEBasis CameraBasis(const TEBasis& centred,double yaw,double pitch,double distance){
  TEBasis basis=centred;basis.RotateX(pitch);basis.RotateY(yaw);basis.SetZoom(centred.GetZoom()*4.4/distance);return basis;
}
inline bool SameBasis(const TEBasis& a,const TEBasis& b){
  auto same=[](double x,double y){return std::isfinite(x)&&std::isfinite(y)&&std::abs(x-y)<=1e-11*std::max(1.,std::max(std::abs(x),std::abs(y)));};
  if(!same(a.GetZoom(),b.GetZoom())||!same(a.GetRX(),b.GetRX())||!same(a.GetRY(),b.GetRY())||!same(a.GetRZ(),b.GetRZ()))return false;
  for(unsigned i=0;i<3;++i){if(!same(a.GetCenter()[i],b.GetCenter()[i]))return false;for(unsigned j=0;j<3;++j)if(!same(a.GetMatrix()[i][j],b.GetMatrix()[i][j]))return false;}return true;
}
inline void ReadCamera(OriginalDocumentViewport& snapshot,const TGlRenderer& renderer,const TEBasis& centred){
  const auto& basis=renderer.GetBasis();
  ViewportRequire(std::isfinite(centred.GetZoom())&&centred.GetZoom()>0&&std::isfinite(basis.GetZoom())&&basis.GetZoom()>0,"Invalid actual original camera zoom");
  snapshot.yaw=basis.GetRY();snapshot.pitch=basis.GetRX();snapshot.distance=centred.GetZoom()*4.4/basis.GetZoom();
  // Inverse zoom arithmetic may cross an exact endpoint by one rounding ULP.
  // Only clamp that arithmetic residue; no caller camera cache is consulted.
  if(snapshot.distance<2.2&&snapshot.distance>=2.2-1e-12)snapshot.distance=2.2;
  if(snapshot.distance>10&&snapshot.distance<=10+1e-12)snapshot.distance=10;
  ViewportRequire(SameBasis(basis,CameraBasis(centred,snapshot.yaw,snapshot.pitch,snapshot.distance)),"Actual original camera has unrepresentable pan/roll/matrix state");
}
}
}
