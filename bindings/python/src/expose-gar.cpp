/// @copyright Copyright (C) 2023 LAAS-CNRS, INRIA
#include "aligator/python/fwd.hpp"
#include "aligator/python/gar-visitors.hpp"
#include "aligator/gar/riccati.hpp"

#include "aligator/python/utils.hpp"
#include "aligator/python/visitors.hpp"

namespace aligator {
namespace python {
using namespace gar;

using context::Scalar;
using prox_riccati_t = ProximalRiccatiSolver<Scalar>;
using knot_t = LQRKnotTpl<Scalar>;
using stage_factor_t = prox_riccati_t::stage_factor_t;
using lqr_t = LQRProblemTpl<Scalar>;
} // namespace python
} // namespace aligator

namespace eigenpy {
namespace internal {
template <>
struct has_operator_equal<::aligator::python::knot_t> : boost::false_type {};
template <>
struct has_operator_equal<::aligator::python::stage_factor_t>
    : boost::false_type {};
} // namespace internal
} // namespace eigenpy

namespace aligator {
namespace python {
using context::MatrixXs;
using RowMatrixXs = Eigen::Transpose<MatrixXs>::PlainMatrix;
using context::VectorXs;

using knot_vec_t = std::vector<knot_t>;

void exposeGAR() {

  bp::scope ns = get_namespace("gar");

  StdVectorPythonVisitor<std::vector<long>, true>::expose("StdVec_long");

  BlkMatrixPythonVisitor<BlkMatrix<MatrixXs, 2, 2>>::expose("BlockMatrix22");
  BlkMatrixPythonVisitor<BlkMatrix<VectorXs, 4, 1>>::expose("BlockVector4");
  BlkMatrixPythonVisitor<BlkMatrix<RowMatrixXs, 4, 1>>::expose(
      "BlockRowMatrix41");

  bp::class_<prox_riccati_t::value_t>("value_data", bp::no_init)
      .def_readonly("Pmat", &prox_riccati_t::value_t::Pmat)
      .def_readonly("pvec", &prox_riccati_t::value_t::pvec)
      .def_readonly("Vmat", &prox_riccati_t::value_t::Vmat)
      .def_readonly("vvec", &prox_riccati_t::value_t::vvec);

  bp::class_<stage_factor_t>("stage_solve_data", bp::no_init)
      .def_readonly("ff", &stage_factor_t::ff)
      .def_readonly("fb", &stage_factor_t::fb)
      .def_readonly("kktMat", &stage_factor_t::kktMat)
      .def_readonly("kktChol", &stage_factor_t::kktChol)
      .def_readonly("vm", &stage_factor_t::vm);

  StdVectorPythonVisitor<std::vector<stage_factor_t>, true>::expose(
      "stage_solve_data_Vec");

  bp::class_<knot_t>("LQRKnot", bp::no_init)
      .def(bp::init<uint, uint, uint>(bp::args("nx", "nu", "nc")))
      .def_readonly("nx", &knot_t::nx)
      .def_readonly("nu", &knot_t::nu)
      .def_readonly("nc", &knot_t::nc)
      //
      .def_readwrite("Q", &knot_t::Q)
      .def_readwrite("S", &knot_t::S)
      .def_readwrite("R", &knot_t::R)
      .def_readwrite("q", &knot_t::q)
      .def_readwrite("r", &knot_t::r)
      //
      .def_readwrite("A", &knot_t::A)
      .def_readwrite("B", &knot_t::B)
      .def_readwrite("E", &knot_t::E)
      .def_readwrite("f", &knot_t::f)
      //
      .def_readwrite("C", &knot_t::C)
      .def_readwrite("D", &knot_t::D)
      .def_readwrite("d", &knot_t::d)
      //
      .def(CopyableVisitor<knot_t>())
      .def(PrintableVisitor<knot_t>());

  StdVectorPythonVisitor<knot_vec_t, true>::expose("LQRKnotVec");

  bp::class_<lqr_t>("LQRProblem", bp::no_init)
      .def(
          bp::init<const knot_vec_t &, long>(bp::args("self", "stages", "nc0")))
      .def_readwrite("stages", &lqr_t::stages)
      .add_property("horizon", &lqr_t::horizon)
      .def_readwrite("G0", &lqr_t::G0)
      .def_readwrite("g0", &lqr_t::g0);

  bp::class_<prox_riccati_t, boost::noncopyable>(
      "ProximalRiccatiSolver", "Proximal Riccati solver.", bp::no_init)
      .def(bp::init<const lqr_t &>(bp::args("self", "knots")))
      .def_readonly("datas", &prox_riccati_t::datas)
      .def("backward", &prox_riccati_t::backward,
           bp::args("self", "mu", "mueq"))
      .def("forward", &prox_riccati_t::forward,
           bp::args("self", "xs", "us", "vs", "lbdas"));

  bp::def(
      "lqrDenseMatrix",
      +[](const lqr_t &problem, Scalar mudyn, Scalar mueq) {
        auto mat_rhs = lqrDenseMatrix(problem, mudyn, mueq);
        return bp::make_tuple(std::get<0>(mat_rhs), std::get<1>(mat_rhs));
      },
      bp::args("self", "mudyn", "mueq"));
}

} // namespace python
} // namespace aligator
