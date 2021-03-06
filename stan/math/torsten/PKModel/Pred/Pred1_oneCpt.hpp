#ifndef STAN_MATH_TORSTEN_PKMODEL_PRED_PRED1_ONECPT_HPP
#define STAN_MATH_TORSTEN_PKMODEL_PRED_PRED1_ONECPT_HPP

#include <stan/math/prim/scal/err/check_finite.hpp>
#include <stan/math/torsten/PKModel/ModelParameters.hpp>
#include <stan/math/prim/mat/fun/Eigen.hpp>
#include <stan/math/torsten/PKModel/Pred/PolyExp.hpp>
#include <iostream>
#include <vector>

namespace torsten {

struct Pred1_oneCpt {
  Pred1_oneCpt() { }

  /**
   * One compartment model with first order absorption
   * Calculates the amount in each compartment at dt time units after the time
   * of the initial condition.
   *
   *  If the initial time equals the time of the event, than the code does
   *	not run the ode integrator, and sets the predicted amount equal to the
   *	initial condition. This can happen when we are dealing with events that
   *	occur simultaneously. The change to the predicted amount caused by bolus
   *	dosing events is handled later in the main Pred function.
   *
   *	 @tparam T_time type of scalar for time
   *	 @tparam T_rate type of scalar for rate
   *	 @tparam T_parameters type of scalar for model parameters
   *	 @tparam T_addParm type of scalar for model parameters
   *	 @param[in] dt time between current and previous event
   *	 @param[in] parameter model parameters at current event
   *	 @param[in] init amount in each compartment at previous event
   *	 @param[in] rate rate in each compartment
   *   @return an eigen vector that contains predicted amount in each compartment
   *           at the current event.
   */
  template<typename T_time, typename T_rate, typename T_parameters,
           typename T_biovar, typename T_tlag, typename T_init>
  Eigen::Matrix<typename boost::math::tools::promote_args<T_time, T_rate,
    T_parameters, T_init>::type, Eigen::Dynamic, 1>
  operator()(const T_time& dt,
             const ModelParameters<T_time, T_parameters, T_biovar,
                                  T_tlag>& parameter,
             const Eigen::Matrix<T_init, 1, Eigen::Dynamic>& init,
             const std::vector<T_rate>& rate) const {
    stan::math::check_finite("Pred1", "initial values", init);

    using std::vector;
    using boost::math::tools::promote_args;
    using Eigen::Matrix;
    using Eigen::Dynamic;

    typedef typename promote_args<T_time, T_rate, T_parameters,
                                  T_init>::type scalar;

    T_parameters CL = parameter.get_RealParameters(false)[0],
      V2 = parameter.get_RealParameters(false)[1],
      ka = parameter.get_RealParameters(false)[2];

    T_parameters k10 = CL / V2;
    vector<T_parameters> alpha(2, 0);
    alpha[0] = k10;
    alpha[1] = ka;

    vector<scalar> a(2, 0);
    Matrix<scalar, 1, Dynamic> pred = Matrix<scalar, 1, Dynamic>::Zero(2);

    if ((init[0] != 0) || (rate[0] != 0)) {
      pred(0, 0) = init[0] * exp(-ka * dt) + rate[0] * (1 - exp(-ka * dt)) / ka;
      a[0] = ka / (ka - alpha[0]);
      a[1] = -a[0];
      pred(0, 1) += PolyExp(dt, init[0], 0, 0, 0, false, a, alpha, 2) +
        PolyExp(dt, 0, rate[0], dt, 0, false, a, alpha, 2);
    }

    if ((init[1] != 0) || (rate[1] != 0)) {
      a[0] = 1;
      pred(0, 1) += PolyExp(dt, init[1], 0, 0, 0, false, a, alpha, 1) +
        PolyExp(dt, 0, rate[1], dt, 0, false, a, alpha, 1);
    }
    return pred;
  }
};

}

#endif
