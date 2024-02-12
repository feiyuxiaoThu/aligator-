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
    if (contact_map_.getContactState(i)) {
      d.value_ += u.template segment<3>(long(i) * 3);
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
    if (contact_map_.getContactState(i)) {
      d.Ju_.template block<3, 3>(0, long(i) * 3).setIdentity();
      d.Ju_.template block<3, 3>(0, long(i) * 3) *= 1 / mass_;
    }
  }
}

template <typename Scalar>
CentroidalAccelerationDataTpl<Scalar>::CentroidalAccelerationDataTpl(
    const CentroidalAccelerationResidualTpl<Scalar> *model)
    : Base(model->ndx1, model->nu, model->ndx2, 3) {}

} // namespace aligator
