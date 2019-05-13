#ifndef STAN_MATH_PRIM_MAT_FUN_MDIVIDE_LEFT_TRI_HPP
#define STAN_MATH_PRIM_MAT_FUN_MDIVIDE_LEFT_TRI_HPP

#include <boost/math/tools/promotion.hpp>
#include <stan/math/prim/mat/fun/Eigen.hpp>
#include <stan/math/prim/mat/fun/promote_common.hpp>
#include <stan/math/prim/mat/err/check_multiplicable.hpp>
#include <stan/math/prim/mat/err/check_square.hpp>
#ifdef STAN_OPENCL
#include <stan/math/opencl/opencl_context.hpp>
#include <stan/math/opencl/multiply.hpp>
#include <stan/math/opencl/lower_tri_inverse.hpp>
#include <stan/math/opencl/transpose.hpp>
#include <stan/math/opencl/copy.hpp>
#endif
namespace stan {
namespace math {

/**
 * Returns the solution of the system Ax=b when A is triangular
 * @param A Triangular matrix.  Specify upper or lower with TriView
 * being Eigen::Upper or Eigen::Lower.
 * @param b Right hand side matrix or vector.
 * @return x = A^-1 b, solution of the linear system.
 * @throws std::domain_error if A is not square or the rows of b don't
 * match the size of A.
 */
template <int TriView, typename T1, typename T2, int R1, int C1, int R2, int C2>
inline Eigen::Matrix<typename boost::math::tools::promote_args<T1, T2>::type,
                     R1, C2>
mdivide_left_tri(const Eigen::Matrix<T1, R1, C1> &A,
                 const Eigen::Matrix<T2, R2, C2> &b) {
  check_square("mdivide_left_tri", "A", A);
  check_multiplicable("mdivide_left_tri", "A", A, "b", b);
  return promote_common<Eigen::Matrix<T1, R1, C1>, Eigen::Matrix<T2, R1, C1> >(
             A)
      .template triangularView<TriView>()
      .solve(
          promote_common<Eigen::Matrix<T1, R2, C2>, Eigen::Matrix<T2, R2, C2> >(
              b));
}

/**
 * Returns the solution of the system Ax=b when A is triangular and b=I.
 * @param A Triangular matrix.  Specify upper or lower with TriView
 * being Eigen::Upper or Eigen::Lower.
 * @return x = A^-1 .
 * @throws std::domain_error if A is not square
 */
template <int TriView, typename T, int R1, int C1>
inline Eigen::Matrix<T, R1, C1> mdivide_left_tri(
    const Eigen::Matrix<T, R1, C1> &A) {
  check_square("mdivide_left_tri", "A", A);
  int n = A.rows();
  Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic> b;
  b.setIdentity(n, n);
  A.template triangularView<TriView>().solveInPlace(b);
  return b;
}

/**
 * Returns the solution of the system Ax=b when A is triangular
 * and A and b are matrices of doubles.
 * @param A Triangular matrix.  Specify upper or lower with TriView
 * being Eigen::Upper or Eigen::Lower.
 * @param b Right hand side matrix or vector.
 * @return x = A^-1 b, solution of the linear system.
 * @throws std::domain_error if A is not square or the rows of b don't
 * match the size of A.
 */
template <int TriView, int R1, int C1, int R2, int C2>
inline Eigen::Matrix<double, R1, C2> mdivide_left_tri(
    const Eigen::Matrix<double, R1, C1> &A,
    const Eigen::Matrix<double, R2, C2> &b) {
  check_square("mdivide_left_tri", "A", A);
  check_multiplicable("mdivide_left_tri", "A", A, "b", b);
#ifdef STAN_OPENCL
  if (A.rows()
      >= opencl_context.tuning_opts().lower_tri_inverse_size_worth_transfer) {
    matrix_cl A_cl(A);
    matrix_cl b_cl(b);
    matrix_cl A_inv_cl(A.rows(), A.cols());
    if (TriView == Eigen::Lower) {
      A_inv_cl = lower_triangular_inverse(A_cl);
    } else {
      A_inv_cl = transpose(lower_triangular_inverse(transpose(A_cl)));
    }
    matrix_cl C_cl = A_inv_cl * b_cl;
    return from_matrix_cl(C_cl);
  } else {
#endif
    return A.template triangularView<TriView>().solve(b);
#ifdef STAN_OPENCL
  }
#endif
}

/**
 * Returns the solution of the system Ax=b when A is triangular, b=I and
 * both are matrices of doubles.
 * @param A Triangular matrix.  Specify upper or lower with TriView
 * being Eigen::Upper or Eigen::Lower.
 * @return x = A^-1 .
 * @throws std::domain_error if A is not square
 */
template <int TriView, int R1, int C1>
inline Eigen::Matrix<double, R1, C1> mdivide_left_tri(
    const Eigen::Matrix<double, R1, C1> &A) {
  check_square("mdivide_left_tri", "A", A);
  const int n = A.rows();
#ifdef STAN_OPENCL
  if (A.rows()
      >= opencl_context.tuning_opts().lower_tri_inverse_size_worth_transfer) {
    matrix_cl A_cl(A);
    if (TriView == Eigen::Lower) {
      A_cl = lower_triangular_inverse(A_cl);
    } else {
      A_cl = transpose(lower_triangular_inverse(transpose(A_cl)));
    }
    return from_matrix_cl(A_cl);
  } else {
#endif
    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> b;
    b.setIdentity(n, n);
    A.template triangularView<TriView>().solveInPlace(b);
    return b;
#ifdef STAN_OPENCL
  }
#endif
}

}  // namespace math
}  // namespace stan
#endif
