#pragma once

#include <crocoddyl/core/optctrl/shooting.hpp>
#include "aligator/core/traj-opt-problem.hpp"
#include "aligator/compat/crocoddyl/cost-wrap.hpp"
#include "aligator/compat/crocoddyl/action-model-wrap.hpp"

namespace aligator {
namespace compat {
namespace croc {

/**
 * @brief   This function converts a crocoddyl::ShootingProblemTpl
 * to an aligator::TrajOptProblemTpl
 *
 * @details
 */
template <typename Scalar>
TrajOptProblemTpl<Scalar> convertCrocoddylProblem(
    const boost::shared_ptr<crocoddyl::ShootingProblemTpl<Scalar>>
        &croc_problem) {
  const crocoddyl::ShootingProblemTpl<Scalar> &cpb = *croc_problem;
  using VectorXs = typename math_types<Scalar>::VectorXs;
  using StageModel = StageModelTpl<Scalar>;
  using ActionModelWrapper = ActionModelWrapperTpl<Scalar>;

  const std::size_t nsteps = cpb.get_T();
  const VectorXs &x0 = cpb.get_x0();

  const auto &running_models = cpb.get_runningModels();

  // construct the std::vector of StageModel to provide the proxddp
  // TrajOptProblem.
  std::vector<shared_ptr<StageModel>> stages;
  stages.reserve(nsteps);
  for (std::size_t i = 0; i < nsteps; i++) {
    stages.push_back(std::make_shared<ActionModelWrapper>(running_models[i]));
  }
  auto converted_cost = std::make_shared<CrocCostModelWrapperTpl<Scalar>>(
      cpb.get_terminalModel());
  TrajOptProblemTpl<Scalar> problem(x0, stages, converted_cost);
  return problem;
}

} // namespace croc
} // namespace compat
} // namespace aligator
