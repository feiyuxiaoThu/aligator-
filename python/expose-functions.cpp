/// @copyright Copyright (C) 2022 LAAS-CNRS, INRIA
#include "proxddp/python/fwd.hpp"
#include "proxddp/python/eigen-member.hpp"

#include "proxddp/python/functions.hpp"
#include "proxddp/modelling/state-error.hpp"
#include "proxddp/modelling/linear-function.hpp"
#include "proxddp/modelling/control-box-function.hpp"
#include "proxddp/modelling/function-xpr-slice.hpp"
#include "proxddp/modelling/linear-function-composition.hpp"
#ifdef PROXDDP_WITH_PINOCCHIO
#include "proxddp/modelling/multibody/frame-placement.hpp"
#include "proxddp/modelling/multibody/frame-velocity.hpp"
#include "proxddp/modelling/multibody/frame-translation.hpp"
#endif

namespace proxddp {
namespace python {

void exposeFunctionBase() {
  using context::ConstMatrixRef;
  using context::ConstVectorRef;
  using context::DynamicsModel;
  using context::FunctionData;
  using context::MatrixXs;
  using context::Scalar;
  using context::StageFunction;
  using context::VectorXs;
  using internal::PyStageFunction;
  using FunctionPtr = shared_ptr<StageFunction>;

  bp::register_ptr_to_python<FunctionPtr>();

  bp::class_<PyStageFunction<>, boost::noncopyable>(
      "StageFunction",
      "Base class for ternary functions f(x,u,x') on a stage of the problem.",
      bp::init<const int, const int, const int, const int>(
          bp::args("self", "ndx1", "nu", "ndx2", "nr")))
      .def(bp::init<const int, const int, const int>(
          bp::args("self", "ndx", "nu", "nr")))
      .def("evaluate", bp::pure_virtual(&StageFunction::evaluate),
           bp::args("self", "x", "u", "y", "data"))
      .def("computeJacobians",
           bp::pure_virtual(&StageFunction::computeJacobians),
           bp::args("self", "x", "u", "y", "data"))
      .def("computeVectorHessianProducts",
           &StageFunction::computeVectorHessianProducts,
           bp::args("self", "x", "u", "y", "lbda", "data"))
      .def_readonly("ndx1", &StageFunction::ndx1, "Current state space.")
      .def_readonly("ndx2", &StageFunction::ndx2, "Next state space.")
      .def_readonly("nu", &StageFunction::nu, "Control dimension.")
      .def_readonly("nr", &StageFunction::nr, "Function codimension.")
      .def(CreateDataPythonVisitor<StageFunction>());

  bp::register_ptr_to_python<shared_ptr<FunctionData>>();

  bp::class_<FunctionData>("FunctionData",
                           "Data struct for holding data about functions.",
                           bp::init<int, int, int, int>(
                               bp::args("self", "ndx1", "nu", "ndx2", "nr")))
      .add_property(
          "value",
          bp::make_getter(&FunctionData::valref_,
                          bp::return_value_policy<bp::return_by_value>()),
          "Function value.")
      .add_property("jac_buffer_",
                    make_getter_eigen_matrix(&FunctionData::jac_buffer_),
                    "Buffer of the full function Jacobian wrt (x,u,y).")
      .add_property(
          "vhp_buffer", make_getter_eigen_matrix(&FunctionData::vhp_buffer_),
          "Buffer of the full function vector-Hessian product wrt (x,u,y).")
      .add_property(
          "Jx",
          bp::make_getter(&FunctionData::Jx_,
                          bp::return_value_policy<bp::return_by_value>()),
          "Jacobian with respect to $x$.")
      .add_property(
          "Ju",
          bp::make_getter(&FunctionData::Ju_,
                          bp::return_value_policy<bp::return_by_value>()),
          "Jacobian with respect to $u$.")
      .add_property(
          "Jy",
          bp::make_getter(&FunctionData::Jy_,
                          bp::return_value_policy<bp::return_by_value>()),
          "Jacobian with respect to $y$.")
      .add_property(
          "Hxx",
          bp::make_getter(&FunctionData::Hxx_,
                          bp::return_value_policy<bp::return_by_value>()),
          "Hessian with respect to $(x, x)$.")
      .add_property(
          "Hxu",
          bp::make_getter(&FunctionData::Hxu_,
                          bp::return_value_policy<bp::return_by_value>()),
          "Hessian with respect to $(x, u)$.")
      .add_property(
          "Hxy",
          bp::make_getter(&FunctionData::Hxy_,
                          bp::return_value_policy<bp::return_by_value>()),
          "Hessian with respect to $(x, y)$.")
      .add_property(
          "Huu",
          bp::make_getter(&FunctionData::Huu_,
                          bp::return_value_policy<bp::return_by_value>()),
          "Hessian with respect to $(u, u)$.")
      .add_property(
          "Huy",
          bp::make_getter(&FunctionData::Huy_,
                          bp::return_value_policy<bp::return_by_value>()),
          "Hessian with respect to $(x, y)$.")
      .add_property(
          "Hyy",
          bp::make_getter(&FunctionData::Hyy_,
                          bp::return_value_policy<bp::return_by_value>()),
          "Hessian with respect to $(y, y)$.")
      .def(PrintableVisitor<FunctionData>())
      .def(ClonePythonVisitor<FunctionData>());

  pinpy::StdVectorPythonVisitor<std::vector<FunctionPtr>, true>::expose(
      "StdVec_StageFunction", "Vector of function objects.");
  pinpy::StdVectorPythonVisitor<std::vector<shared_ptr<FunctionData>>, true>::
      expose("StdVec_FunctionData", "Vector of function data objects.");

  bp::class_<StateErrorResidualTpl<Scalar>, bp::bases<StageFunction>>(
      "StateErrorResidual", bp::init<const shared_ptr<context::Manifold> &,
                                     const int, const context::VectorXs &>(
                                bp::args("self", "space", "nu", "target")))
      .def_readonly("space", &StateErrorResidualTpl<Scalar>::space_)
      .def_readwrite("target", &StateErrorResidualTpl<Scalar>::target_);

  bp::class_<ControlErrorResidual<Scalar>, bp::bases<StageFunction>>(
      "ControlErrorResidual",
      bp::init<const int, const shared_ptr<context::Manifold> &,
               const context::VectorXs &>(
          bp::args("self", "ndx", "uspace", "target")))
      .def(bp::init<const int, const context::VectorXs &>(
          bp::args("self", "ndx", "target")))
      .def(bp::init<int, int>(bp::args("self", "ndx", "nu")))
      .def_readonly("space", &ControlErrorResidual<Scalar>::space_)
      .def_readwrite("target", &ControlErrorResidual<Scalar>::target_);

  bp::class_<LinearFunctionTpl<Scalar>, bp::bases<StageFunction>>(
      "LinearFunction", bp::init<const int, const int, const int, const int>(
                            bp::args("self", "ndx1", "nu", "ndx2", "nr")))
      .def(bp::init<const ConstMatrixRef, const ConstMatrixRef,
                    const ConstMatrixRef, const ConstVectorRef>(
          "Constructor with given matrices.",
          bp::args("self", "A", "B", "C", "d")))
      .def(bp::init<const ConstMatrixRef, const ConstMatrixRef,
                    const ConstVectorRef>("Constructor with C=0.",
                                          bp::args("self", "A", "B", "d")))
      .def_readonly("A", &LinearFunctionTpl<Scalar>::A_)
      .def_readonly("B", &LinearFunctionTpl<Scalar>::B_)
      .def_readonly("C", &LinearFunctionTpl<Scalar>::C_)
      .def_readonly("d", &LinearFunctionTpl<Scalar>::d_);

  bp::class_<ControlBoxFunctionTpl<Scalar>, bp::bases<StageFunction>>(
      "ControlBoxFunction",
      bp::init<const int, const VectorXs &, const VectorXs &>(
          bp::args("self", "ndx", "umin", "umax")))
      .def(bp::init<const int, const int, const Scalar, const Scalar>(
          bp::args("self", "ndx", "nu", "umin", "umax")));
}

#ifdef PROXDDP_WITH_PINOCCHIO
void exposePinocchioFunctions() {
  using context::Manifold;
  using context::Scalar;
  using context::StageFunction;
  using Model = pinocchio::ModelTpl<Scalar>;
  using PinData = pinocchio::DataTpl<Scalar>;
  using SE3 = pinocchio::SE3Tpl<Scalar>;
  using Motion = pinocchio::MotionTpl<Scalar>;

  using FramePlacement = FramePlacementResidualTpl<Scalar>;
  using FramePlacementData = FramePlacementDataTpl<Scalar>;

  using FrameVelocity = FrameVelocityResidualTpl<Scalar>;
  using FrameVelocityData = FrameVelocityDataTpl<Scalar>;

  using FrameTranslation = FrameTranslationResidualTpl<Scalar>;
  using FrameTranslationData = FrameTranslationDataTpl<Scalar>;

  bp::register_ptr_to_python<shared_ptr<PinData>>();

  bp::class_<FramePlacement, bp::bases<StageFunction>>(
      "FramePlacementResidual", "Frame placement residual function.",
      bp::init<int, int, shared_ptr<Model>, const SE3 &, pinocchio::FrameIndex>(
          bp::args("self", "ndx", "nu", "model", "p_ref", "id")))
      .add_property("frame_id", &FramePlacement::getFrameId,
                    &FramePlacement::setFrameId)
      .def("getReference", &FramePlacement::getReference, bp::args("self"),
           bp::return_internal_reference<>(), "Get the target frame in SE3.")
      .def("setReference", &FramePlacement::setReference,
           bp::args("self", "p_new"), "Set the target frame in SE3.");

  bp::register_ptr_to_python<shared_ptr<FramePlacementData>>();

  bp::class_<FramePlacementData, bp::bases<context::FunctionData>>(
      "FramePlacementData", "Data struct for FramePlacementResidual.",
      bp::no_init)
      .def_readonly("rMf", &FramePlacementData::rMf_, "Frame placement error.")
      .def_readonly("rJf", &FramePlacementData::rJf_)
      .def_readonly("fJf", &FramePlacementData::fJf_)
      .def_readonly("pin_data", &FramePlacementData::pin_data_,
                    "Pinocchio data struct.");

  bp::class_<FrameVelocity, bp::bases<StageFunction>>(
      "FrameVelocityResidual", "Frame velocity residual function.",
      bp::init<int, int, shared_ptr<Model>, const Motion &,
               pinocchio::FrameIndex, pinocchio::ReferenceFrame>(bp::args(
          "self", "ndx", "nu", "model", "v_ref", "id", "reference_frame")))
      .add_property("frame_id", &FrameVelocity::getFrameId,
                    &FrameVelocity::setFrameId)
      .def("getReference", &FrameVelocity::getReference, bp::args("self"),
           bp::return_internal_reference<>(), "Get the target frame velocity.")
      .def("setReference", &FrameVelocity::setReference,
           bp::args("self", "v_new"), "Set the target frame velocity.");

  bp::register_ptr_to_python<shared_ptr<FrameVelocityData>>();

  bp::class_<FrameVelocityData, bp::bases<context::FunctionData>>(
      "FrameVelocityData", "Data struct for FrameVelocityResidual.",
      bp::no_init)
      .def_readonly("pin_data", &FrameVelocityData::pin_data_,
                    "Pinocchio data struct.");

  bp::class_<FrameTranslation, bp::bases<StageFunction>>(
      "FrameTranslationResidual", "Frame placement residual function.",
      bp::init<int, int, shared_ptr<Model>, const context::VectorXs &,
               pinocchio::FrameIndex>(
          bp::args("self", "ndx", "nu", "model", "p_ref", "id")))
      .add_property("frame_id", &FrameTranslation::getFrameId,
                    &FrameTranslation::setFrameId)
      .def("getReference", &FrameTranslation::getReference, bp::args("self"),
           bp::return_internal_reference<>(),
           "Get the target frame translation.")
      .def("setReference", &FrameTranslation::setReference,
           bp::args("self", "p_new"), "Set the target frame translation.");

  bp::register_ptr_to_python<shared_ptr<FrameTranslationData>>();

  bp::class_<FrameTranslationData, bp::bases<context::FunctionData>>(
      "FrameTranslationData", "Data struct for FrameTranslationResidual.",
      bp::no_init)
      .def_readonly("fJf", &FrameTranslationData::fJf_)
      .def_readonly("pin_data", &FrameTranslationData::pin_data_,
                    "Pinocchio data struct.");
}
#endif

void exposeFunctionExpressions() {
  using context::FunctionData;
  using context::Scalar;
  using context::StageFunction;

  using FunctionPtr = shared_ptr<StageFunction>;

  // FUNCTION SLICE

  using FunctionSliceXpr = FunctionSliceXprTpl<Scalar>;

  bp::class_<FunctionSliceXpr, bp::bases<StageFunction>>(
      "FunctionSliceXpr",
      "Represents a slice of an expression according to either a single index "
      "or an array of indices.",
      bp::init<FunctionPtr, std::vector<int>>(
          bp::args("self", "func", "indices")))
      .def(bp::init<FunctionPtr, const int>("Constructor from a single index.",
                                            bp::args("self", "func", "idx")))
      .def(CreateDataPythonVisitor<FunctionSliceXpr>());

  bp::class_<FunctionSliceXpr::OwnData, bp::bases<FunctionData>,
             boost::noncopyable>("FunctionSliceData", bp::no_init)
      .def_readonly("sub_data", &FunctionSliceXpr::OwnData::sub_data,
                    "Underlying function's data.");

  /// FUNCTION LINEAR COMPOSE

  using LinearFunctionComposition = LinearFunctionCompositionTpl<Scalar>;

  bp::class_<LinearFunctionComposition, bp::bases<StageFunction>>(
      "LinearFunctionComposition",
      "Function composition :math:`r(x) = Af(x) + b`.",
      bp::init<FunctionPtr, const context::MatrixXs, const context::VectorXs>(
          "Construct a composition from the underlying function, weight matrix "
          ":math:`A` and bias :math:`b`.",
          bp::args("self", "func", "A", "b")))
      .def(bp::init<FunctionPtr, const context::MatrixXs>(
          "Constructor where the bias :math:`b` is assumed to be zero.",
          bp::args("self", "func", "A")))
      .def_readonly("func", &LinearFunctionComposition::func,
                    "The underlying function.")
      .add_property("A",
                    make_getter_eigen_matrix(&LinearFunctionComposition::A),
                    "Weight matrix.")
      .add_property("b",
                    make_getter_eigen_matrix(&LinearFunctionComposition::A),
                    "Bias vector.");

  bp::class_<LinearFunctionComposition::OwnData, bp::bases<FunctionData>,
             boost::noncopyable>("LinearFunctionCompositionData", bp::no_init)
      .def_readonly("sub_data", &LinearFunctionComposition::OwnData::sub_data);
}

void exposeFunctions() {
  exposeFunctionBase();
  exposePinocchioFunctions();
  exposeFunctionExpressions();
}

} // namespace python
} // namespace proxddp
