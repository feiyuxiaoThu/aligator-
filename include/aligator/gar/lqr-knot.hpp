/// @copyright Copyright (C) 2023 LAAS-CNRS, INRIA
#pragma once

#include "aligator/math.hpp"

namespace aligator {
namespace gar {

template <typename Scalar> struct LQRKnot {
  ALIGATOR_DYNAMIC_TYPEDEFS(Scalar);

  uint nx, nu, nc;
  MatrixXs Q, S, R;
  VectorXs q, r;
  MatrixXs A, B, E;
  VectorXs f;
  MatrixXs C, D;
  VectorXs d;

  LQRKnot(uint nx, uint nu, uint nc)
      : nx(nx), nu(nu), nc(nc),                        //
        Q(nx, nx), S(nx, nu), R(nu, nu), q(nx), r(nu), //
        A(nx, nx), B(nx, nu), E(nx, nx), f(nx),        //
        C(nc, nx), D(nc, nu), d(nc) {
    Q.setZero();
    S.setZero();
    R.setZero();
    q.setZero();
    r.setZero();

    A.setZero();
    B.setZero();
    E.setZero();
    f.setZero();

    C.setZero();
    D.setZero();
    d.setZero();
  }
};

template <typename T> struct LQRProblem {
  // last stage should have nu = 0
  std::vector<LQRKnot<T>> stages;

  size_t horizon() const noexcept { return stages.size(); }
};

template <typename Scalar>
auto lqrDenseMatrix(const std::vector<LQRKnot<Scalar>> &knots, Scalar mudyn,
                    Scalar mueq) {

  using MatrixXs = typename math_types<Scalar>::MatrixXs;
  using VectorXs = typename math_types<Scalar>::VectorXs;
  using knot_t = LQRKnot<Scalar>;
  size_t N = knots.size() - 1UL;
  uint nrows = 0;
  for (size_t t = 0; t < knots.size(); t++) {
    const knot_t &model = knots[t];
    nrows += model.nx + model.nu + model.nc;
    if (t != N)
      nrows += model.nx;
  }

  MatrixXs mat(nrows, nrows);
  mat.setZero();
  VectorXs rhs(nrows);

  uint idx = 0;
  for (size_t t = 0; t <= N; t++) {
    const knot_t &model = knots[t];
    // get block for current variables
    uint n = model.nx + model.nu + model.nc;
    auto block = mat.block(idx, idx, n, n);
    auto rhsblk = rhs.segment(idx, n);
    auto Q = block.topLeftCorner(model.nx, model.nx);
    auto St = block.leftCols(model.nx).middleRows(model.nx, model.nu);
    auto R = block.block(model.nx, model.nx, model.nu, model.nu);
    auto C = block.bottomRows(model.nc).leftCols(model.nx);
    auto D = block.bottomRows(model.nc).middleCols(model.nx, model.nu);
    auto dual = block.bottomRightCorner(model.nc, model.nc).diagonal();
    dual.array() = -mueq;

    Q = model.Q;
    St = model.S.transpose();
    R = model.R;
    C = model.C;
    D = model.D;

    block = block.template selfadjointView<Eigen::Lower>();

    rhsblk.head(model.nx) = model.q;
    rhsblk.segment(model.nx, model.nu) = model.r;
    rhsblk.tail(model.nc) = model.d;

    // fill in dynamics
    // row contains [A; B; 0; -mu*I, E] -> nx + nu + nc + 2*nx cols
    if (t != N) {
      auto row = mat.block(idx + n, idx, model.nx, model.nx * 2 + n);
      row.leftCols(model.nx) = model.A;
      row.middleCols(model.nx, model.nu) = model.B;
      row.middleCols(n, model.nx).diagonal().array() = -mudyn;
      row.rightCols(model.nx) = model.E;

      rhs.segment(idx + n, model.nx) = model.f;

      auto col =
          mat.transpose().block(idx + n, idx, model.nx, model.nx * 2 + n);
      col = row;

      // shift by size of block + multiplier size
      idx += model.nx + n;
    }
  }
  return std::make_pair(mat, rhs);
}

} // namespace gar
} // namespace aligator

#ifdef ALIGATOR_ENABLE_TEMPLATE_INSTANTIATION
#include "./lqr-knot.txx"
#endif
