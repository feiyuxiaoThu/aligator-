#include "proxddp/python/fwd.hpp"

#include "proxddp/modelling/dynamics/multibody-constraint-fwd.hpp"

namespace proxddp {
namespace python {
namespace context {

using RigidConstraintModel = pinocchio::RigidConstraintModelTpl<Scalar, 0>;
using RigidConstraintData = pinocchio::RigidConstraintDataTpl<Scalar, 0>;

using RigidConstraintModelVector =
    PINOCCHIO_STD_VECTOR_WITH_EIGEN_ALLOCATOR(context::RigidConstraintModel);
using RigidConstraintDataVector =
    PINOCCHIO_STD_VECTOR_WITH_EIGEN_ALLOCATOR(context::RigidConstraintData);

} // namespace context

void exposeConstraintFwdDynamics() {
  using namespace proxddp::dynamics;
  using context::Scalar;
  using ODEData = ODEDataTpl<Scalar>;
  using ODEAbstract = ODEAbstractTpl<Scalar>;
  using context::RigidConstraintModelVector;
  using context::RigidConstraintDataVector;
  using MultibodyConstraintFwdData = MultibodyConstraintFwdDataTpl<Scalar>;
  using MultibodyConstraintFwdDynamics =
      MultibodyConstraintFwdDynamicsTpl<Scalar>;

  bp::class_<MultibodyConstraintFwdDynamics, bp::bases<ODEAbstract>>(
      "MultibodyConstraintFwdDynamics",
      "Constraint forward dynamics using Pinocchio.",
      bp::init<const shared_ptr<proxnlp::MultibodyPhaseSpace<Scalar>> &,
               const context::MatrixXs &, const RigidConstraintModelVector &,
               const pinocchio::ProximalSettingsTpl<Scalar> &>(
          bp::args("self", "space", "actuation_matrix", "constraint_models",
                   "prox_settings")))
      .add_property("ntau", &MultibodyConstraintFwdDynamics::ntau,
                    "Torque dimension.")
      .def(CreateDataPythonVisitor<MultibodyConstraintFwdDynamics>());

  bp::register_ptr_to_python<shared_ptr<MultibodyConstraintFwdData>>();

  bp::class_<MultibodyConstraintFwdData, bp::bases<ODEData>>(
      "MultibodyConstraintFwdData", bp::no_init)
      .def_readwrite("tau", &MultibodyConstraintFwdData::tau_)
      .def_readwrite("dtau_du", &MultibodyConstraintFwdData::dtau_du_)
      .def_readwrite("pin_data", &MultibodyConstraintFwdData::pin_data_)
      .def_readwrite("constraint_models",
                     &MultibodyConstraintFwdData::constraint_models_)
      .def_readwrite("constraint_datas",
                     &MultibodyConstraintFwdData::constraint_datas_);
}
} // namespace python
} // namespace proxddp