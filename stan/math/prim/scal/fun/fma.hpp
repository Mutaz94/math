#ifndef STAN_MATH_PRIM_SCAL_FUN_FMA_HPP
#define STAN_MATH_PRIM_SCAL_FUN_FMA_HPP

#include <stan/math/prim/meta.hpp>
#include <cmath>

namespace stan {
namespace math {

/**
 * Return the product of the first two arguments plus the third
 * argument.
 *
 * <p><i>Warning:</i> This does not delegate to the high-precision
 * platform-specific <code>fma()</code> implementation.
 *
 * @param x First argument.
 * @param y Second argument.
 * @param z Third argument.
 * @return The product of the first two arguments plus the third
 * argument.
 */
template <typename T1, typename T2, typename T3,
          typename = enable_if_all_arithmetic<T1, T2, T3>>
inline auto fma(T1&& x, T2&& y, T3&& z) {
  using std::fma;
  return fma(std::forward<T1>(x), std::forward<T2>(y),
             std::forward<T3>(z));
}

}  // namespace math
}  // namespace stan
#endif
