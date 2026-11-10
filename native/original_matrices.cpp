#include "original_matrices.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace olex2::harmony {
namespace {
float Checked(double value) {
  if (!std::isfinite(value) || std::abs(value)>std::numeric_limits<float>::max())
    throw std::invalid_argument("Nonfinite or unrepresentable matrix coefficient");
  return static_cast<float>(value);
}
void Validate(const Matrix4& value) { for(float element:value) (void)Checked(element); }
}
Matrix4 IdentityMatrix() { return {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}; }
Matrix4 MultiplyMatrices(const Matrix4& a,const Matrix4& b) {
  Validate(a);Validate(b);Matrix4 output{};
  for(size_t column=0;column<4;++column) for(size_t row=0;row<4;++row) {
    double sum=0;
    for(size_t k=0;k<4;++k) sum+=static_cast<double>(a[k*4+row])*b[column*4+k];
    output[column*4+row]=Checked(sum);
  }
  return output;
}
Matrix4 OwnedBasisMatrix(const TEBasis& basis,bool transposed) {
  // Validate double fields before the original narrowing assignments.
  for(size_t r=0;r<3;++r) {
    (void)Checked(basis.GetCenter()[r]);
    for(size_t c=0;c<3;++c) (void)Checked(basis.GetMatrix()[r][c]);
  }
  Matrix4 output{};
  if(transposed) basis.CopyMDataT(output.data()); else basis.CopyMData(output.data());
  Validate(output);return output;
}
Matrix4 NormalTransform(const Matrix4& matrix) {
  Validate(matrix);
  if(matrix[3]!=0 || matrix[7]!=0 || matrix[11]!=0 || matrix[15]!=1)
    throw std::invalid_argument("Normal transform requires an affine model-view matrix");
  double a[3][3],scale=0;
  for(size_t r=0;r<3;++r) for(size_t c=0;c<3;++c) {
    a[r][c]=matrix[c*4+r];scale=std::max(scale,std::abs(a[r][c]));
  }
  const double cof[3][3]={
    {a[1][1]*a[2][2]-a[1][2]*a[2][1],a[1][2]*a[2][0]-a[1][0]*a[2][2],a[1][0]*a[2][1]-a[1][1]*a[2][0]},
    {a[0][2]*a[2][1]-a[0][1]*a[2][2],a[0][0]*a[2][2]-a[0][2]*a[2][0],a[0][1]*a[2][0]-a[0][0]*a[2][1]},
    {a[0][1]*a[1][2]-a[0][2]*a[1][1],a[0][2]*a[1][0]-a[0][0]*a[1][2],a[0][0]*a[1][1]-a[0][1]*a[1][0]}};
  const double determinant=a[0][0]*cof[0][0]+a[0][1]*cof[0][1]+a[0][2]*cof[0][2];
  if(scale==0 || !std::isfinite(determinant) || std::abs(determinant)/(scale*scale*scale)<1.e-12)
    throw std::invalid_argument("Singular or ill-conditioned normal transform");
  auto output=IdentityMatrix();
  for(size_t r=0;r<3;++r) for(size_t c=0;c<3;++c) output[c*4+r]=Checked(cof[r][c]/determinant);
  return output;
}
MatrixState::MatrixState():modelView_(1,IdentityMatrix()),projection_(1,IdentityMatrix()) {}
void MatrixState::SetMode(MatrixMode mode) { (void)Stack(mode);mode_=mode; }
const std::vector<Matrix4>& MatrixState::Stack(MatrixMode mode) const {
  switch(mode) {case MatrixMode::ModelView:return modelView_;case MatrixMode::Projection:return projection_;}
  throw std::invalid_argument("Unsupported matrix mode");
}
std::vector<Matrix4>& MatrixState::Active() {
  return mode_==MatrixMode::ModelView?modelView_:projection_;
}
void MatrixState::LoadIdentity() { Active().back()=IdentityMatrix(); }
void MatrixState::Load(const Matrix4& matrix) { Validate(matrix);Active().back()=matrix; }
void MatrixState::Multiply(const Matrix4& matrix) {
  const auto next=MultiplyMatrices(Current(),matrix);Active().back()=next;
}
void MatrixState::Orient(const TEBasis& basis,bool transposed) { Multiply(OwnedBasisMatrix(basis,transposed)); }
void MatrixState::Translate(double x,double y,double z) {
  auto value=IdentityMatrix();value[12]=Checked(x);value[13]=Checked(y);value[14]=Checked(z);Multiply(value);
}
void MatrixState::Scale(double x,double y,double z) {
  auto value=IdentityMatrix();value[0]=Checked(x);value[5]=Checked(y);value[10]=Checked(z);Multiply(value);
}
void MatrixState::Rotate(double degrees,double x,double y,double z) {
  const double length=std::hypot(x,y,z);
  if(!std::isfinite(degrees) || !std::isfinite(length) || length<1.e-12)
    throw std::invalid_argument("Rotation requires a finite angle and defined axis");
  x/=length;y/=length;z/=length;
  const double radians=degrees*(std::acos(-1.)/180.);
  const double c=std::cos(radians),s=std::sin(radians),t=1-c;
  auto value=IdentityMatrix();
  value[0]=Checked(t*x*x+c);value[4]=Checked(t*x*y-s*z);value[8]=Checked(t*x*z+s*y);
  value[1]=Checked(t*x*y+s*z);value[5]=Checked(t*y*y+c);value[9]=Checked(t*y*z-s*x);
  value[2]=Checked(t*x*z-s*y);value[6]=Checked(t*y*z+s*x);value[10]=Checked(t*z*z+c);
  Multiply(value);
}
void MatrixState::Push() {
  auto& stack=Active();if(stack.size()>=MaxDepth) throw std::length_error("Matrix stack budget exceeded");
  const auto copy=stack.back();stack.push_back(copy);
}
void MatrixState::Pop() {
  auto& stack=Active();if(stack.size()==1) throw std::underflow_error("Cannot pop matrix stack base");stack.pop_back();
}
Matrix4 MatrixState::Current() const { return Stack(mode_).back(); }
Matrix4 MatrixState::ModelView() const { return modelView_.back(); }
Matrix4 MatrixState::Projection() const { return projection_.back(); }
Matrix4 MatrixState::Combined() const { return MultiplyMatrices(Projection(),ModelView()); }
size_t MatrixState::Depth(MatrixMode mode) const { return Stack(mode).size(); }
OriginalCameraMatrices BuildOriginalCamera(float yaw,float pitch,float distance,unsigned width,unsigned height) {
  if(!std::isfinite(yaw) || !std::isfinite(pitch) || !std::isfinite(distance) || distance<=0 || width==0 || height==0 || width>16384 || height>16384)
    throw std::invalid_argument("Invalid original-basis camera parameters");
  TEBasis basis;
  // TEBasis exports the transpose of its row storage. This order and pitch
  // sign preserve the existing documented world-Y then local-X view controls.
  basis.RotateX(-static_cast<double>(pitch));basis.RotateY(static_cast<double>(yaw));
  const auto owned=OwnedBasisMatrix(basis);
  bool original=true;
  for(size_t c=0;c<3;++c) for(size_t r=0;r<3;++r)
    original=original && owned[c*4+r]==static_cast<float>(basis.GetMatrix()[c][r]);
  MatrixState state;
  state.Translate(0,0,-static_cast<double>(distance));state.Orient(basis);
  Matrix4 projection{};
  const float aspect=static_cast<float>(width)/height,f=1.f/std::tan(.7f/2.f);
  projection[0]=f/aspect;projection[5]=f;projection[10]=-100.1f/99.9f;projection[11]=-1.f;projection[14]=-.2f*100.f/99.9f;
  state.SetMode(MatrixMode::Projection);state.Load(projection);state.SetMode(MatrixMode::ModelView);
  OriginalCameraMatrices output;
  output.modelView=state.ModelView();output.mvp=state.Combined();output.normal=NormalTransform(output.modelView);
  output.modelViewDepth=static_cast<unsigned>(state.Depth(MatrixMode::ModelView));
  output.projectionDepth=static_cast<unsigned>(state.Depth(MatrixMode::Projection));output.originalBasisVerified=original;
  if(!original) throw std::runtime_error("Owned basis export differs from original matrix fields");
  return output;
}
}
