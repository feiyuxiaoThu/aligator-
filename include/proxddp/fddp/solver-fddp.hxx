#pragma once

#include "proxddp/fddp/solver-fddp.hpp"

namespace proxddp {

/* SolverFDDP<Scalar> */

template <typename Scalar>
SolverFDDP<Scalar>::SolverFDDP(const Scalar tol, VerboseLevel verbose,
                               const Scalar reg_init)
    : target_tol_(tol), xreg_(reg_init), ureg_(reg_init), verbose_(verbose) {}

template <typename Scalar>
void SolverFDDP<Scalar>::setup(const Problem &problem) {
  results_ = std::make_unique<Results>(problem);
  workspace_ = std::make_unique<Workspace>(problem);
  // check if there are any constraints other than dynamics and throw a warning
  std::vector<std::size_t> idx_where_constraints;
  for (std::size_t i = 0; i < problem.numSteps(); i++) {
    const shared_ptr<StageModel> &sm = problem.stages_[i];
    if (sm->numConstraints() > 1) {
      idx_where_constraints.push_back(i);
    }
  }
  if (idx_where_constraints.size() > 0) {
    proxddp_fddp_warning(
        fmt::format("problem stages [{}] have constraints, "
                    "which this solver cannot handle. "
                    "Please use a penalized cost formulation.\n",
                    fmt::join(idx_where_constraints, ", ")));
  }
  if (problem.term_constraint_) {
    proxddp_fddp_warning(
        "problem has a terminal constraint, which this solver cannot "
        "handle.\n");
  }
}

template <typename Scalar>
void SolverFDDP<Scalar>::forwardPass(const Problem &problem,
                                     const Results &results,
                                     Workspace &workspace, const Scalar alpha) {
  const std::size_t nsteps = workspace.nsteps;
  std::vector<VectorXs> &xs_try = workspace.trial_xs_;
  std::vector<VectorXs> &us_try = workspace.trial_us_;
  std::vector<VectorXs> &xnexts = workspace.xnexts_;
  const std::vector<VectorXs> &fs = workspace.feas_gaps_;
  ProblemData &pd = workspace.problem_data;

  {
    const Manifold &space = problem.stages_[0]->xspace();
    space.integrate(results.xs_[0], alpha * fs[0], xs_try[0]);
  }

  for (std::size_t i = 0; i < nsteps; i++) {
    const StageModel &sm = *problem.stages_[i];
    StageData &sd = pd.getData(i);

    ConstVectorRef ff = results.getFeedforward(i);
    ConstMatrixRef fb = results.getFeedback(i);

    sm.xspace().difference(results.xs_[i], xs_try[i], workspace.dxs_[i]);
    sm.uspace().integrate(results.us_[i], alpha * ff + fb * workspace.dxs_[i],
                          us_try[i]);
    sm.evaluate(xs_try[i], us_try[i], xs_try[i + 1], sd);
    ExpData &dd = stage_get_dynamics_data(sd);
    xnexts[i + 1] = dd.xnext_;
    sm.xspace_next().integrate(xnexts[i + 1], fs[i + 1] * (alpha - 1.),
                               xs_try[i + 1]);
  }
  const Manifold &space = problem.stages_.back()->xspace();
  space.difference(results.xs_[nsteps], xs_try[nsteps], workspace.dxs_[nsteps]);
#ifndef NDEBUG
  if (alpha == 0.)
    assert(math::infty_norm(workspace.dxs_) <=
           std::numeric_limits<Scalar>::epsilon());
#endif
}

template <typename Scalar>
void SolverFDDP<Scalar>::computeDirectionalDerivatives(Workspace &workspace,
                                                       Results &results,
                                                       Scalar &dgrad,
                                                       Scalar &dquad) const {
  const std::size_t nsteps = workspace.nsteps;
  const std::vector<VectorXs> &fs = workspace.feas_gaps_;
  dgrad = 0.; // cost directional derivative
  dquad = 0.;

  assert(workspace.q_params.size() == nsteps);
  assert(workspace.value_params.size() == (nsteps + 1));
  for (std::size_t i = 0; i < nsteps; i++) {
    const QParams &qpar = workspace.q_params[i];
    ConstVectorRef Qu = qpar.Qu_;
    ConstVectorRef ff = results.getFeedforward(i);
    dgrad += Qu.dot(ff);
    dquad += ff.dot(workspace.Quuks_[i]);
  }
  for (std::size_t i = 0; i <= nsteps; i++) {
    const VParams &vpar = workspace.value_params[i];
    const VectorXs &ftVxx = workspace.ftVxx_[i];
    dgrad += vpar.Vx_.dot(fs[i]);
    dquad -= ftVxx.dot(fs[i]);
  }
}

template <typename Scalar>
void SolverFDDP<Scalar>::directionalDerivativeCorrection(const Problem &problem,
                                                         Workspace &workspace,
                                                         Results &results,
                                                         Scalar &d1,
                                                         Scalar &d2) {
  const std::size_t nsteps = workspace.nsteps;
  const std::vector<VectorXs> &xs = results.xs_;
  const std::vector<VectorXs> &us = results.us_;

  Scalar dv = 0.;
  for (std::size_t i = 0; i <= nsteps; i++) {
    const VParams &vpar = workspace.value_params[i];
    const VectorXs &ftVxx = workspace.ftVxx_[i];
    dv += ftVxx.dot(workspace.dxs_[i]);
  }
#ifndef NDEBUG
  fmt::print("dv = {:.5g}\n", dv);
#endif

  d1 += -dv;
  d2 += 2 * dv;
}

template <typename Scalar>
Scalar SolverFDDP<Scalar>::computeInfeasibility(const Problem &problem,
                                                const std::vector<VectorXs> &xs,
                                                const std::vector<VectorXs> &us,
                                                Workspace &workspace) {
  const std::size_t nsteps = workspace.nsteps;
  const ProblemData &pd = workspace.problem_data;
  std::vector<VectorXs> &xnexts = workspace.xnexts_;
  std::vector<VectorXs> &fs = workspace.feas_gaps_;

  const VectorXs &x0 = problem.getInitState();
  const Manifold &space = problem.stages_[0]->xspace();
  space.difference(xs[0], x0, fs[0]);
  xnexts[0] = x0;

  for (std::size_t i = 0; i < nsteps; i++) {
    const StageModel &sm = *problem.stages_[i];
    const ExpData &dd = stage_get_dynamics_data(pd.getData(i));
    xnexts[i + 1] = dd.xnext_;
    sm.xspace().difference(xs[i + 1], xnexts[i + 1], fs[i + 1]);
  }
  problem.term_cost_->evaluate(xs[nsteps], problem.dummy_term_u0,
                               *pd.term_cost_data);
  return math::infty_norm(workspace.feas_gaps_);
}

template <typename Scalar>
void SolverFDDP<Scalar>::computeCriterion(Workspace &workspace,
                                          Results &results) {
  const std::size_t nsteps = workspace.nsteps;
  std::vector<ConstVectorRef> Qus;
  for (std::size_t i = 0; i < nsteps; i++) {
    Qus.push_back(workspace.q_params[i].Qu_);
  }
  results.dual_infeasibility = math::infty_norm(Qus);
}

template <typename Scalar>
void SolverFDDP<Scalar>::backwardPass(const Problem &problem,
                                      Workspace &workspace,
                                      Results &results) const {

  const std::size_t nsteps = workspace.nsteps;
  const std::vector<VectorXs> &fs = workspace.feas_gaps_;

  ProblemData &prob_data = workspace.problem_data;
  {
    const CostData &term_cost_data = *prob_data.term_cost_data;
    VParams &vp = workspace.value_params[nsteps];
    vp.v_2() = 2 * term_cost_data.value_;
    vp.Vx_ = term_cost_data.Lx_;
    vp.Vxx_ = term_cost_data.Lxx_;
    vp.Vxx_.diagonal().array() += xreg_;
    VectorXs &ftVxx = workspace.ftVxx_[nsteps];
    ftVxx.noalias() = vp.Vxx_ * fs[nsteps];
    vp.Vx_ += ftVxx;
    vp.storage = vp.storage.template selfadjointView<Eigen::Lower>();
  }

  std::size_t i;
  for (std::size_t k = 0; k < nsteps; k++) {
    i = nsteps - k - 1;
    const VParams &vnext = workspace.value_params[i + 1];
    QParams &qparam = workspace.q_params[i];

    StageModel &sm = *problem.stages_[i];
    StageData &sd = prob_data.getData(i);

    const int nu = sm.nu();
    const int ndx1 = sm.ndx1();
    const int nt = ndx1 + nu;
    assert((qparam.storage.cols() == nt + 1) &&
           (qparam.storage.rows() == nt + 1));
    assert(qparam.grad_.size() == nt);

    const CostData &cd = *sd.cost_data;
    DynamicsDataTpl<Scalar> &dd = stage_get_dynamics_data(sd);

    /* Assemble Q-function */
    ConstMatrixRef J_x_u = dd.jac_buffer_.leftCols(ndx1 + nu);

    qparam.q_2() = 2 * cd.value_;
    qparam.grad_ = cd.grad_;
    qparam.hess_ = cd.hess_;

    // TODO: implement second-order derivatives for the Q-function
    qparam.grad_.noalias() += J_x_u.transpose() * vnext.Vx_;
    qparam.hess_.noalias() += J_x_u.transpose() * vnext.Vxx_ * J_x_u;
    qparam.Qxx_.diagonal().array() += xreg_;
    qparam.Quu_.diagonal().array() += ureg_;
    qparam.storage = qparam.storage.template selfadjointView<Eigen::Lower>();

    /* Compute gains */
    // MatrixXs &kkt_mat = workspace.kkt_matrix_bufs[i];
    MatrixXs &kkt_rhs = workspace.kkt_rhs_bufs[i];

    // kkt_mat = qparam.Quu_;
    VectorRef ffwd = results.getFeedforward(i);
    MatrixRef fback = results.getFeedback(i);
    ffwd = -qparam.Qu_;
    fback = -qparam.Qxu_.transpose();
    kkt_rhs = results.gains_[i];

    Eigen::LLT<MatrixXs> &llt = workspace.llts_[i];
    llt.compute(qparam.Quu_);
    llt.solveInPlace(results.gains_[i]);

    workspace.Quuks_[i] = qparam.Quu_ * ffwd;

    /* Compute value function */
    VParams &vp = workspace.value_params[i];
    vp.Vx_ = qparam.Qx_ + fback.transpose() * qparam.Qu_;
    vp.Vxx_ = qparam.Qxx_ + qparam.Qxu_ * fback;
    vp.Vxx_.diagonal().array() += xreg_;
    VectorXs &ftVxx = workspace.ftVxx_[i];
    ftVxx.noalias() = vp.Vxx_ * fs[i];
    vp.Vx_ += ftVxx;
    vp.storage = vp.storage.template selfadjointView<Eigen::Lower>();
  }
  assert(i == 0);
}

template <typename Scalar>
bool SolverFDDP<Scalar>::run(const Problem &problem,
                             const std::vector<VectorXs> &xs_init,
                             const std::vector<VectorXs> &us_init) {

  const Scalar fd_eps = 1e-7;

  if (results_ == 0 || workspace_ == 0) {
    proxddp_runtime_error(
        "Either results or workspace not allocated. Call setup() first!");
  }
  Results &results = *results_;
  Workspace &workspace = *workspace_;

  checkTrajectoryAndAssign(problem, xs_init, us_init, results.xs_, results.us_);

  ::proxddp::BaseLogger logger{};
  logger.active = verbose_ > 0;
  logger.start();

  // in Crocoddyl, linesearch xs is primed to use problem x0
  {
    const VectorXs &x0 = problem.getInitState();
    workspace.xnexts_[0] = x0;
  }

  auto linesearch_fun = [&](const Scalar alpha) {
    forwardPass(problem, results, workspace, alpha);
    problem.evaluate(workspace.trial_xs_, workspace.trial_us_,
                     workspace.trial_prob_data);
    return computeTrajectoryCost(problem, workspace.trial_prob_data);
  };

  LogRecord record;
  record.inner_crit = 0.;
  record.dual_err = 0.;
  record.dphi0 = 0.;
  std::size_t &iter = results.num_iters;
  for (iter = 0; iter <= MAX_ITERS; ++iter) {

    record.iter = iter + 1;

    problem.evaluate(results.xs_, results.us_, workspace.problem_data);
    results.traj_cost_ = computeTrajectoryCost(problem, workspace.problem_data);
    results.primal_infeasibility =
        computeInfeasibility(problem, results.xs_, results.us_, workspace);
    problem.computeDerivatives(results.xs_, results.us_,
                               workspace.problem_data);

    backwardPass(problem, workspace, results);
    computeCriterion(workspace, results);

    PROXDDP_RAISE_IF_NAN(results.primal_infeasibility);
    PROXDDP_RAISE_IF_NAN(results.dual_infeasibility);
    record.prim_err = results.primal_infeasibility;
    record.dual_err = results.dual_infeasibility;
    record.merit = results.traj_cost_;
    record.inner_crit = 0.;
    record.xreg = xreg_;

    if (results.dual_infeasibility < target_tol_) {
      results.conv = true;
      break;
    }

    if (iter >= MAX_ITERS) {
      break;
    }

    Scalar phi0 = results.traj_cost_;
    Scalar d1_phi, d2_phi;
    computeDirectionalDerivatives(workspace, results, d1_phi, d2_phi);
    PROXDDP_RAISE_IF_NAN(d1_phi);
    PROXDDP_RAISE_IF_NAN(d2_phi);
#ifndef NDEBUG
    linearRollout(problem, workspace, results);
    directionalDerivativeCorrection(problem, workspace, results, d1_phi,
                                    d2_phi);
    {
      Scalar phi_eps = linesearch_fun(fd_eps);
      fmt::print("phi_eps = {:.5g}\n", phi_eps);
      Scalar finite_diff_d1 = (phi_eps - phi0) / fd_eps;
      fmt::print("finite_diff = {:.5g} vs an = {:.5g} (err = {:.5g})\n",
                 finite_diff_d1, d1_phi, d1_phi - finite_diff_d1);
      assert(math::scalar_close(finite_diff_d1, d1_phi, std::pow(fd_eps, 0.5)));
    }
#endif
    record.dphi0 = d1_phi;

    // quadratic model lambda; captures by copy
    auto ls_model = [=, &problem, &workspace, &results](const Scalar alpha) {
      Scalar d1 = d1_phi;
      Scalar d2 = d2_phi;
      directionalDerivativeCorrection(problem, workspace, results, d1, d2);
      return phi0 + alpha * (d1 + 0.5 * d2 * alpha);
    };

    Scalar alpha_opt = 1;
    bool d1_small = std::abs(d1_phi) < th_grad_;
    if (!d1_small) {
      FDDPGoldsteinLinesearch<Scalar>::run(linesearch_fun, ls_model, phi0,
                                           verbose_, ls_params, alpha_opt);
      record.step_size = alpha_opt;
    }
    Scalar phi_new = linesearch_fun(alpha_opt);
    PROXDDP_RAISE_IF_NAN(phi_new);
    record.merit = phi_new;
    record.dM = phi_new - phi0;

    results.xs_ = workspace.trial_xs_;
    results.us_ = workspace.trial_us_;
    if (d1_small) {
      results.conv = true;
      logger.log(record);
      break;
    }

    if (alpha_opt > th_step_dec_) {
      decreaseRegularization();
    }
    if (alpha_opt <= th_step_inc_) {
      increaseRegularization();
      if (xreg_ == reg_max_) {
        results.conv = false;
        break;
      }
    }

    invokeCallbacks(workspace, results);
    logger.log(record);
  }

  logger.finish(results.conv);
  return results.conv;
}
} // namespace proxddp