#pragma once
#include "ebasis.h"
#include <array>
#include <vector>
namespace olex2::harmony {
using Matrix4=std::array<float,16>;
Matrix4 IdentityMatrix();
Matrix4 MultiplyMatrices(const Matrix4& left,const Matrix4& right);
// Uses the shared original assignments, but never borrows FMData/FMDataT.
Matrix4 OwnedBasisMatrix(const TEBasis& basis,bool transposed=false);
Matrix4 NormalTransform(const Matrix4& modelView);
enum class MatrixMode { ModelView, Projection };
// Bounded explicit state for the new backend, not an emulation of every GL
// matrix enum, error flag, query, double-precision path or display-list state.
class MatrixState {
public:
  static constexpr size_t MaxDepth=32;
  MatrixState();
  void SetMode(MatrixMode mode);
  void LoadIdentity();
  void Load(const Matrix4& matrix);
  void Multiply(const Matrix4& matrix); // current = current * matrix
  void Orient(const TEBasis& basis,bool transposed=false);
  void Translate(double x,double y,double z);
  void Scale(double x,double y,double z);
  void Rotate(double degrees,double x,double y,double z);
  void Push();
  void Pop();
  Matrix4 Current() const;
  Matrix4 ModelView() const;
  Matrix4 Projection() const;
  Matrix4 Combined() const;
  size_t Depth(MatrixMode mode) const;
private:
  MatrixMode mode_=MatrixMode::ModelView;
  std::vector<Matrix4> modelView_,projection_;
  std::vector<Matrix4>& Active();
  const std::vector<Matrix4>& Stack(MatrixMode mode) const;
};
struct OriginalCameraMatrices {
  Matrix4 modelView{},mvp{},normal{};
  unsigned modelViewDepth=0,projectionDepth=0;
  bool originalBasisVerified=false;
};
OriginalCameraMatrices BuildOriginalCamera(float yaw,float pitch,float distance,unsigned width,unsigned height);
constexpr const char* OriginalMatrixSource() {
  return "upstream/sdl/ebasis.h/.cpp: original TEBasis RotateX/RotateY and owned CopyMData";
}
}
