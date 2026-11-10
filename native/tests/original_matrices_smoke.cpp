// Synthetic coordinates/matrices only. General stack and inverse-transpose
// cases are CPU evidence, not GPU coverage of every legacy rendering state.
#include "original_matrices.h"
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
namespace {
using namespace olex2::harmony;
unsigned checks=0,poses=0;
void Check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message);++checks; }
bool Near(double a,double b,double tolerance=2.e-5) { return std::abs(a-b)<tolerance; }
template<class F> void Reject(F action,const char* message) {
  bool rejected=false;try {action();} catch(const std::exception&) {rejected=true;}Check(rejected,message);
}
std::array<double,4> Point(const Matrix4& m,const std::array<double,4>& p) {
  std::array<double,4> out{};
  for(size_t r=0;r<4;++r) for(size_t c=0;c<4;++c) out[r]+=m[c*4+r]*p[c];return out;
}
void OwnedOriginalExports() {
  TEBasis b;b.RotateX(-22);b.RotateY(32);b.SetCenter(vec3d(3,-2,.25));
  auto copy=OwnedBasisMatrix(b);auto transposed=OwnedBasisMatrix(b,true);
  const float* legacy=b.GetMData();bool same=true;
  for(size_t i=0;i<16;++i) same=same && copy[i]==legacy[i];
  Check(same,"owned direct export preserves every legacy coefficient");
  const float* legacyT=b.GetMDataT();same=true;
  for(size_t i=0;i<16;++i) same=same && transposed[i]==legacyT[i];
  Check(same,"owned transposed export preserves every legacy coefficient");
  Check(copy[12]==3 && copy[13]==-2 && copy[14]==.25f && copy[3]==0 && copy[15]==1,"original translation and homogeneous layout preserved");
  Check(transposed[12]==3 && transposed[13]==-2 && transposed[14]==.25f && transposed[3]==0,"original transposed export does not transpose translation into the last row");
  b.SetZoom(7);Check(copy==OwnedBasisMatrix(b),"original export does not implicitly apply separate zoom");
  TEBasis other;other.RotateY(75);other.SetCenter(vec3d(-1,2,0));auto otherCopy=OwnedBasisMatrix(other);
  Check(legacy==other.GetMData(),"legacy pointer aliasing remains compatible");
  Check(copy!=otherCopy && copy==OwnedBasisMatrix(b),"owned snapshots do not borrow shared static buffers");
  std::atomic<bool> independent{true};
  auto worker=[&](const TEBasis& basis,const Matrix4& expected) {
    for(unsigned i=0;i<2000;++i) if(OwnedBasisMatrix(basis)!=expected) independent=false;
  };
  std::thread first(worker,std::cref(b),std::cref(copy)),second(worker,std::cref(other),std::cref(otherCopy));
  first.join();second.join();Check(independent,"read-only original exports to separate owned buffers remain independent");
  auto invalid=b;invalid.SetCenter(vec3d(std::numeric_limits<double>::infinity(),0,0));
  Reject([&]{OwnedBasisMatrix(invalid);},"target rejects nonfinite exported basis");
  invalid=b;invalid.SetCenter(vec3d(std::numeric_limits<double>::max(),0,0));
  Reject([&]{OwnedBasisMatrix(invalid);},"target rejects basis coefficients not representable by float");
}
void StateAndNormals() {
  MatrixState s;Check(s.Current()==IdentityMatrix() && s.Depth(MatrixMode::ModelView)==1 && s.Depth(MatrixMode::Projection)==1,"two explicit identity stack bases");
  s.Translate(1,2,3);s.Rotate(90,0,0,2);auto point=Point(s.Current(),{1,0,0,1});
  Check(Near(point[0],1) && Near(point[1],3) && Near(point[2],3),"postmultiply translate/rotate order and normalized axis");
  auto parent=s.Current();s.Push();s.Scale(2,3,4);s.Pop();Check(s.Current()==parent,"nested local transform restores parent");
  s.SetMode(MatrixMode::Projection);s.Scale(2,2,2);auto projection=s.Current();
  Check(s.ModelView()==parent,"projection mode does not overwrite model-view");
  auto p1=Point(parent,{.25,-.5,1,1});auto p2=Point(projection,p1);auto actual=Point(s.Combined(),{.25,-.5,1,1});
  Check(Near(actual[0],p2[0]) && Near(actual[1],p2[1]) && Near(actual[2],p2[2]) && Near(actual[3],p2[3]),"projection times model-view composition");
  s.Push();s.LoadIdentity();s.Pop();Check(s.Current()==projection,"projection push/pop restores its own parent");
  s.SetMode(MatrixMode::ModelView);s.LoadIdentity();s.Rotate(120,1,1,1);point=Point(s.Current(),{1,0,0,1});
  Check(Near(point[0],0) && Near(point[1],1) && Near(point[2],0),"arbitrary-axis rotation orientation");
  s.LoadIdentity();s.Translate(7,-3,2);s.Scale(2,3,4);auto normal=NormalTransform(s.Current());
  Check(Near(normal[0],.5) && Near(normal[5],1./3) && Near(normal[10],.25) && normal[12]==0 && normal[13]==0 && normal[14]==0,"inverse-transpose normal scaling ignores translation");
  auto shear=IdentityMatrix();shear[4]=2;normal=NormalTransform(shear);
  auto transformedNormal=Point(normal,{1,0,0,0}),tangent=Point(shear,{0,1,0,0});
  Check(Near(transformedNormal[0]*tangent[0]+transformedNormal[1]*tangent[1]+transformedNormal[2]*tangent[2],0),"normal remains perpendicular after shear");
  s.LoadIdentity();s.Scale(-2,3,4);normal=NormalTransform(s.Current());Check(Near(normal[0],-.5),"reflection sign preserved in normal transform");
  s.LoadIdentity();auto before=s.Current();Reject([&]{s.Pop();},"cannot pop stack base");Check(s.Current()==before,"underflow leaves state intact");
  for(size_t i=1;i<MatrixState::MaxDepth;++i) s.Push();
  Reject([&]{s.Push();},"explicit stack depth budget");Check(s.Depth(MatrixMode::ModelView)==MatrixState::MaxDepth,"overflow does not add a matrix");
  for(size_t i=1;i<MatrixState::MaxDepth;++i) s.Pop();
  Reject([&]{s.SetMode(static_cast<MatrixMode>(99));},"unsupported matrix mode is rejected");Check(s.Current()==before,"invalid mode leaves current mode intact");
  auto bad=IdentityMatrix();bad[0]=std::numeric_limits<float>::quiet_NaN();Reject([&]{s.Load(bad);},"nonfinite load rejected");Check(s.Current()==before,"failed load is transactional");
  Reject([&]{s.Rotate(10,0,0,0);},"undefined rotation axis rejected");
  Reject([&]{s.Translate(std::numeric_limits<double>::infinity(),0,0);},"nonfinite translation rejected");
  Check(s.Current()==before,"failed elementary operations do not mutate state");
  s.Scale(std::numeric_limits<float>::max(),1,1);before=s.Current();Reject([&]{s.Scale(2,1,1);},"multiplication overflow rejected");Check(s.Current()==before,"failed multiplication is transactional");
  bad=IdentityMatrix();bad[0]=0;Reject([&]{NormalTransform(bad);},"singular normal transform rejected");
  bad=IdentityMatrix();bad[3]=.1f;Reject([&]{NormalTransform(bad);},"projective model-view normal transform unsupported explicitly");
  Reject([&]{BuildOriginalCamera(0,0,4,0,100);},"zero-size viewport rejected");
  Reject([&]{BuildOriginalCamera(0,0,0,100,100);},"invalid camera distance rejected");
  Reject([&]{BuildOriginalCamera(std::numeric_limits<float>::infinity(),0,4,100,100);},"nonfinite camera pose rejected");
}
void IndependentCameraPoses() {
  for(float yaw:{-170.f,-90.f,-15.f,0.f,32.f,90.f,170.f})
    for(float pitch:{-80.f,-22.f,0.f,55.f,80.f})
      for(float distance:{2.2f,4.4f,10.f})
        for(const auto size:{std::array<unsigned,2>{320,640},std::array<unsigned,2>{934,296},std::array<unsigned,2>{1280,720}}) {
          const auto camera=BuildOriginalCamera(yaw,pitch,distance,size[0],size[1]);
          const double y=yaw*std::acos(-1.)/180.,x=pitch*std::acos(-1.)/180.;
          const double cy=std::cos(y),sy=std::sin(y),cx=std::cos(x),sx=std::sin(x);
          const double expected[3][3]={{cy,sy*sx,sy*cx},{0,cx,-sx},{-sy,cy*sx,cy*cx}};
          bool same=camera.originalBasisVerified && camera.modelViewDepth==1 && camera.projectionDepth==1;
          for(size_t r=0;r<3;++r) for(size_t c=0;c<3;++c)
            same=same && Near(camera.modelView[c*4+r],expected[r][c]) && Near(camera.normal[c*4+r],expected[r][c]);
          same=same && camera.modelView[12]==0 && camera.modelView[13]==0 && camera.modelView[14]==-distance && camera.normal[12]==0;
          for(const auto p:{std::array<double,4>{.25,-.5,.75,1},std::array<double,4>{-.7,.3,-.2,1},std::array<double,4>{0,0,0,1}}) {
            double rotated[3]{};for(size_t r=0;r<3;++r) for(size_t c=0;c<3;++c) rotated[r]+=expected[r][c]*p[c];
            const double z=rotated[2]-distance,f=1/std::tan(.35),aspect=double(size[0])/size[1];
            const auto actual=Point(camera.mvp,p);
            same=same && Near(actual[0],f/aspect*rotated[0]) && Near(actual[1],f*rotated[1]) &&
              Near(actual[2],-100.1/99.9*z-.2*100/99.9) && Near(actual[3],-z);
          }
          Check(same,"original basis camera differs from independent analytic pose/clip reference");++poses;
        }
}
}
int main() {
  try {
    OwnedOriginalExports();StateAndNormals();IndependentCameraPoses();
    std::cout<<"PASS original basis matrix state; "<<checks<<" assertions; "<<poses<<" independent camera poses; general stacks/scales/shears are CPU evidence, not complete GL state\n";return 0;
  } catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
