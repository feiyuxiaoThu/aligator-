#pragma once
/// @file ode-abstract.hpp
/// @brief Defines a class representing ODEs.

#include "proxddp/modelling/dynamics/continuous-base.hpp"


namespace proxddp
{
  namespace dynamics
  {
    /** @brief   Base class for ODE dynamics \f$ \dot{x} = f(x, u) \f$.
     * @details  Formulated as a DAE (for ContinuousDynamicsAbstractTpl), this class models
     *           \f[
     *              f(x, u) - \dot{x}.
     *           \f]
     */            
    template<typename _Scalar>
    struct ODEAbstractTpl : ContinuousDynamicsAbstractTpl<_Scalar>
    {
      using Scalar = _Scalar;
      PROXNLP_DYNAMIC_TYPEDEFS(Scalar);

      using Base = ContinuousDynamicsAbstractTpl<_Scalar>;
      using BaseData = typename Base::Data;
      using Base::Base; // use parent ctor

      using Data = ODEDataTpl<Scalar>;

      /// Evaluate the ODE vector field: this returns the value of \f$\dot{x}\f$.
      virtual void forward(const ConstVectorRef& x, const ConstVectorRef& u, Data& data) const = 0;

      /// Evaluate the vector field Jacobians.
      virtual void dForward(const ConstVectorRef& x, const ConstVectorRef& u, Data& data) const = 0;

      /** Declare overrides **/

      void evaluate(const ConstVectorRef& x,
                    const ConstVectorRef& u,
                    const ConstVectorRef& xdot,
                    BaseData& data) const;

      void computeJacobians(const ConstVectorRef& x,
                            const ConstVectorRef& u,
                            const ConstVectorRef& xdot,
                            BaseData& data) const;

      virtual shared_ptr<BaseData> createData() const
      {
        return std::make_shared<Data>(this->ndx(), this->nu());
      }

    };

    template<typename _Scalar>
    struct ODEDataTpl : ContinuousDynamicsDataTpl<_Scalar>
    {
      using Scalar = _Scalar;
      PROXNLP_DYNAMIC_TYPEDEFS(Scalar);
      using Base = ContinuousDynamicsDataTpl<Scalar>;

      /// Value of the derivative \f$\dot{x}\f$
      VectorXs xdot_;

      ODEDataTpl(const int ndx, const int nu)
        : Base(ndx, nu)
        , xdot_(ndx)
      {
        xdot_.setZero();
      }
    };
    
  } // namespace dynamics
} // namespace proxddp

#include "proxddp/modelling/dynamics/ode-abstract.hxx"
