#pragma once

#include "aligator/modelling/centroidal/centroidal-acceleration.hpp"

namespace aligator {

template <typename Scalar>
void CentroidalAccelerationResidualTpl<Scalar>::evaluate(
    const ConstVectorRef &, const ConstVectorRef &u, const ConstVectorRef &,
    BaseData &data) const {
  Data &d = static_cast<Data &>(data);

  d.value_.setZero();
  for (std::size_t i = 0; i < nk_; i++) {
    const auto &it = contact_map_[i];
    if (it.first) {
      d.value_ += u.segment(i * 3, 3);
    }
  }

  d.value_ /= mass_;
  d.value_ += gravity_;
}

template <typename Scalar>
void CentroidalAccelerationResidualTpl<Scalar>::computeJacobians(
    const ConstVectorRef &, const ConstVectorRef &, const ConstVectorRef &,
    BaseData &data) const {
  Data &d = static_cast<Data &>(data);

  d.Ju_.setZero();
  for (std::size_t i = 0; i < nk_; i++) {
    const auto &it = contact_map_[i];
    if (it.first) {
      d.Ju_.block(0, i * 3, 3, 3).setIdentity();
      d.Ju_.block(0, i * 3, 3, 3) *= 1 / mass_;
    }
  }
}

template <typename Scalar>
CentroidalAccelerationDataTpl<Scalar>::CentroidalAccelerationDataTpl(
    const CentroidalAccelerationResidualTpl<Scalar> *model)
    : Base(model->ndx1, model->nu, model->ndx2, 3) {}

} // namespace aligator
