#ifndef STAN_MATH_OPENCL_KERNEL_GENERATOR_MULTI_RESULT_KERNEL_HPP
#define STAN_MATH_OPENCL_KERNEL_GENERATOR_MULTI_RESULT_KERNEL_HPP
#ifdef STAN_OPENCL

#include <stan/math/opencl/kernel_generator/is_valid_expression.hpp>
#include <stan/math/opencl/kernel_generator/name_generator.hpp>
#include <stan/math/opencl/kernel_generator/as_operation_cl.hpp>
#include <stan/math/opencl/kernel_generator/calc_if.hpp>
#include <stan/math/opencl/kernel_generator/load.hpp>
#include <stan/math/opencl/opencl_context.hpp>
#include <string>
#include <tuple>
#include <utility>
#include <set>

namespace stan {
namespace math {

namespace internal {

/**
 * A wrapper for references. Used to wrap references when putting them in
 * tuples.
 */
template <typename T>
struct wrapper {
  T x;
  explicit wrapper(T&& x) : x(std::forward<T>(x)) {}
};

template <typename T>
wrapper<T> make_wrapper(T&& x) {
  return wrapper<T>(std::forward<T>(x));
}

// Pack can only be at the end of the list. We need 2 packs, so we nest structs.
template <int n, typename... T_results>
struct multi_result_kernel_internal {
  template <typename... T_expressions>
  struct inner {
    static cl::Kernel kernel_;
    using next = typename multi_result_kernel_internal<
        n - 1, T_results...>::template inner<T_expressions...>;
    using T_current_result = std::remove_reference_t<
        std::tuple_element_t<n, std::tuple<T_results...>>>;
    using T_current_expression = std::remove_reference_t<
        std::tuple_element_t<n, std::tuple<T_expressions...>>>;
    static void check_assign_dimensions(
        int n_rows, int n_cols,
        const std::tuple<wrapper<T_results>...>& results,
        const std::tuple<wrapper<T_expressions>...>& expressions) {
      next::check_assign_dimensions(n_rows, n_cols, results, expressions);
      const auto& expression = std::get<n>(expressions).x;
      const auto& result = std::get<n>(results).x;
      const char* function = "results.operator=";
      check_size_match(function, "Rows of ", "expression",
                       expression.thread_rows(), "rows of ", "first expression",
                       n_rows);
      check_size_match(function, "Columns of ", "expression",
                       expression.thread_cols(), "columns of ",
                       "first expression", n_cols);
      if (!is_no_output<T_current_expression>::value) {
        result.check_assign_dimensions(expression.rows(), expression.cols());
        result.set_view(expression.bottom_diagonal(), expression.top_diagonal(),
                        1 - expression.rows(), expression.cols() - 1);
      }
    }

    static kernel_parts generate(
        std::set<const operation_cl_base*>& generated, name_generator& ng,
        const std::string& i, const std::string& j,
        const std::tuple<wrapper<T_results>...>& results,
        const std::tuple<wrapper<T_expressions>...>& expressions) {
      kernel_parts parts
          = next::generate(generated, ng, i, j, results, expressions);
      if (is_no_output<T_current_expression>::value) {
        return parts;
      }
      kernel_parts parts0
          = std::get<n>(expressions)
                .x.get_whole_kernel_parts(generated, ng, i, j,
                                          std::get<n>(results).x);
      parts.initialization += parts0.initialization;
      parts.body += parts0.body;
      parts.reduction += parts0.reduction;
      parts.args += parts0.args;
      return parts;
    }

    static void set_args(
        std::set<const operation_cl_base*>& generated, cl::Kernel& kernel,
        int& arg_num, const std::tuple<wrapper<T_results>...>& results,
        const std::tuple<wrapper<T_expressions>...>& expressions) {
      next::set_args(generated, kernel, arg_num, results, expressions);

      if (is_no_output<T_current_expression>::value) {
        return;
      }

      std::get<n>(expressions).x.set_args(generated, kernel, arg_num);
      std::get<n>(results).x.set_args(generated, kernel, arg_num);
    }

    static void add_event(
        cl::Event e, const std::tuple<wrapper<T_results>...>& results,
        const std::tuple<wrapper<T_expressions>...>& expressions) {
      next::add_event(e, results, expressions);

      std::get<n>(expressions).x.add_read_event(e);
      std::get<n>(results).x.add_write_event(e);
    }
  };
};

template <typename... T_results>
struct multi_result_kernel_internal<-1, T_results...> {
  template <typename... T_expressions>
  struct inner {
    static void check_assign_dimensions(
        int n_rows, int n_cols,
        const std::tuple<wrapper<T_results>...>& results,
        const std::tuple<wrapper<T_expressions>...>& expressions) {
      return;
    }

    static kernel_parts generate(
        std::set<const operation_cl_base*>& generated, name_generator& ng,
        const std::string& i, const std::string& j,
        const std::tuple<wrapper<T_results>...>& results,
        const std::tuple<wrapper<T_expressions>...>& expressions) {
      return {};
    }

    static void set_args(
        std::set<const operation_cl_base*>& generated, cl::Kernel& kernel,
        int& arg_num, const std::tuple<wrapper<T_results>...>& results,
        const std::tuple<wrapper<T_expressions>...>& expressions) {
      return;
    }

    static void add_event(
        cl::Event e, const std::tuple<wrapper<T_results>...>& results,
        const std::tuple<wrapper<T_expressions>...>& expressions) {
      return;
    }
  };
};

template <int n, typename... T_results>
template <typename... T_expressions>
cl::Kernel multi_result_kernel_internal<n, T_results...>::inner<
    T_expressions...>::kernel_;

}  // namespace internal

/**
 * Represents multiple expressions that will be calculated in same kernel.
 * @tparam T_expressions types of expressions
 */
template <typename... T_expressions>
class expressions_cl {
 public:
  /**
   * Constructor.
   * @param expressions expressions that will be calculated in same kernel.
   */
  explicit expressions_cl(T_expressions&&... expressions)
      : expressions_(internal::wrapper<T_expressions>(
            std::forward<T_expressions>(expressions))...) {}

 private:
  std::tuple<internal::wrapper<T_expressions>...> expressions_;
  template <typename... T_results>
  friend class results_cl;
};

/**
 * Deduces types for constructing \c expressions_cl object.
 * @tparam T_expressions types of expressions
 * @param expressions expressions that will be used in same kernel.
 */
template <typename... T_expressions>
expressions_cl<T_expressions...> expressions(T_expressions&&... expressions) {
  return expressions_cl<T_expressions...>(
      std::forward<T_expressions>(expressions)...);
}

/**
 * Represents results that will be calculated in same kernel.
 * @tparam T_results types of results
 */
template <typename... T_results>
class results_cl {
  std::tuple<internal::wrapper<T_results>...> results_;

 public:
  /**
   * Constructor.
   * @param results results that will be calculated in same kernel
   */
  explicit results_cl(T_results&&... results)
      : results_(
            internal::wrapper<T_results>(std::forward<T_results>(results))...) {
  }

  /**
   * Assigning \c expressions_cl object to \c results_ object generates and
   * executes the kernel that evaluates expressions and stores them into result
   * expressions this object contains.
   * @tparam T_expressions types of expressions
   * @param expressions expressions
   */
  template <typename... T_expressions,
            typename = std::enable_if_t<sizeof...(T_results)
                                        == sizeof...(T_expressions)>>
  void operator=(const expressions_cl<T_expressions...>& expressions) {
    assignment(expressions,
               std::make_index_sequence<sizeof...(T_expressions)>{});
  }

  /**
   * Generates kernel source for evaluating given expressions into results held
   * by \c this.
   * @tparam T_expressions types of expressions
   * @param expressions expressions to generate kernel source for
   * @return kernel source
   */
  template <typename... T_expressions,
            typename = std::enable_if_t<sizeof...(T_results)
                                        == sizeof...(T_expressions)>>
  std::string get_kernel_source_for_evaluating(
      const expressions_cl<T_expressions...>& expressions) {
    return get_kernel_source_for_evaluating_impl(
        expressions, std::make_index_sequence<sizeof...(T_expressions)>{});
  }

 private:
  /**
   * Implementation of kernel soource generation.
   * @tparam T_expressions types of expressions
   * @tparam Is indices
   * @param exprs expressions
   */
  template <typename... T_expressions, size_t... Is>
  std::string get_kernel_source_for_evaluating_impl(
      const expressions_cl<T_expressions...>& exprs,
      std::index_sequence<Is...>) {
    auto expressions = std::make_tuple(
        internal::make_wrapper(std::forward<decltype(as_operation_cl(
                                   std::get<Is>(exprs.expressions_).x))>(
            as_operation_cl(std::get<Is>(exprs.expressions_).x)))...);
    auto results = std::make_tuple(internal::make_wrapper(
        std::forward<decltype(as_operation_cl(std::get<Is>(results_).x))>(
            as_operation_cl(std::get<Is>(results_).x)))...);
    return get_kernel_source_impl(results, expressions);
  }

  /**
   * Implementation of kernel soource generation.
   * @tparam T_res types of results
   * @tparam T_expressions types of expressions
   * @param results results
   * @param expressions expressions
   */
  template <typename... T_res, typename... T_expressions>
  static std::string get_kernel_source_impl(
      const std::tuple<internal::wrapper<T_res>...>& results,
      const std::tuple<internal::wrapper<T_expressions>...>& expressions) {
    using impl = typename internal::multi_result_kernel_internal<
        std::tuple_size<std::tuple<T_expressions...>>::value - 1,
        T_res...>::template inner<T_expressions...>;
    static const bool require_specific_local_size = std::max(
        {std::decay_t<T_expressions>::Deriv::require_specific_local_size...});

    name_generator ng;
    std::set<const operation_cl_base*> generated;
    kernel_parts parts
        = impl::generate(generated, ng, "i", "j", results, expressions);
    std::string src;
    if (require_specific_local_size) {
      src =
          "kernel void calculate(" + parts.args +
          "const int rows, const int cols){\n"
          "const int gid_i = get_global_id(0);\n"
          "const int lid_i = get_local_id(0);\n"
          "const int lsize_i = get_local_size(0);\n"
          "const int wg_id_i = get_group_id(0);\n"
          "const int wg_id_j = get_group_id(1);\n"
          "const int n_groups_i = get_num_groups(0);\n"
          "const int blocks_rows = (rows + lsize_i - 1) / lsize_i;\n"
          "const int blocks_cols = (cols + lsize_i - 1) / lsize_i;\n"
          "const int i0 = lsize_i * wg_id_i;\n"
          "const int i = i0 + lid_i;\n"
          "const int j0 = lsize_i * wg_id_j;\n"
          "for(int lid_j = 0; lid_j < min(cols - j0, lsize_i); lid_j++){\n"
          "const int j = j0 + lid_j;\n"
          + parts.initialization +
          "if(i < rows){\n"
          + parts.body +
          "}\n"
          + parts.reduction +
          "}\n"
          "}\n";
    } else {
      src =
          "kernel void calculate(" +
          parts.args.substr(0, parts.args.size() - 2) +
          "){\n"
          "int i = get_global_id(0);\n"
          "int j = get_global_id(1);\n"
          + parts.initialization
          + parts.body
          + parts.reduction +
          "}\n";
    }
    return src;
  }

  /**
   * Assignment of expressions to results.
   * @tparam T_expressions types of expressions
   * @param exprs expressions
   */
  template <typename... T_expressions, size_t... Is>
  void assignment(const expressions_cl<T_expressions...>& exprs,
                  std::index_sequence<Is...>) {
    auto expressions = std::make_tuple(
        internal::make_wrapper(std::forward<decltype(as_operation_cl(
                                   std::get<Is>(exprs.expressions_).x))>(
            as_operation_cl(std::get<Is>(exprs.expressions_).x)))...);
    auto results = std::make_tuple(internal::make_wrapper(
        std::forward<decltype(as_operation_cl(std::get<Is>(results_).x))>(
            as_operation_cl(std::get<Is>(results_).x)))...);
    assignment_impl(results, expressions);
  }

  /**
   * Implementation of assignments of expressions to results
   * @tparam T_res types of results
   * @tparam T_expressions types of expressions
   * @param results results
   * @param expressions expressions
   */
  template <typename... T_res, typename... T_expressions>
  static void assignment_impl(
      const std::tuple<internal::wrapper<T_res>...>& results,
      const std::tuple<internal::wrapper<T_expressions>...>& expressions) {
    using T_First_Expr = typename std::remove_reference_t<
        std::tuple_element_t<0, std::tuple<T_expressions...>>>;
    using impl = typename internal::multi_result_kernel_internal<
        std::tuple_size<std::tuple<T_expressions...>>::value - 1,
        T_res...>::template inner<T_expressions...>;

    static const bool require_specific_local_size = std::max(
        {std::decay_t<T_expressions>::Deriv::require_specific_local_size...});

    int n_rows = std::get<0>(expressions).x.thread_rows();
    int n_cols = std::get<0>(expressions).x.thread_cols();
    const char* function = "results_cl.assignment";
    check_positive(function, "number of rows", n_rows);
    check_positive(function, "number of columns", n_cols);
    impl::check_assign_dimensions(n_rows, n_cols, results, expressions);

    try {
      if (impl::kernel_() == NULL) {
        std::string src = get_kernel_source_impl(results, expressions);
        auto opts = opencl_context.base_opts();
        impl::kernel_ = opencl_kernels::compile_kernel(
            "calculate", {view_kernel_helpers, src}, opts);
      }
      cl::Kernel& kernel = impl::kernel_;
      int arg_num = 0;

      std::set<const operation_cl_base*> generated;
      impl::set_args(generated, kernel, arg_num, results, expressions);

      cl::Event e;
      if (require_specific_local_size) {
        kernel.setArg(arg_num++, n_rows);
        kernel.setArg(arg_num++, n_cols);
        int local = opencl_context.base_opts().at("LOCAL_SIZE_");
        int wgs_rows = (n_rows + local - 1) / local;
        int wgs_cols = (n_cols + local - 1) / local;

        opencl_context.queue().enqueueNDRangeKernel(
            kernel, cl::NullRange, cl::NDRange(local * wgs_rows, wgs_cols),
            cl::NDRange(local, 1), nullptr, &e);
      } else {
        opencl_context.queue().enqueueNDRangeKernel(kernel, cl::NullRange,
                                                    cl::NDRange(n_rows, n_cols),
                                                    cl::NullRange, nullptr, &e);
      }
      impl::add_event(e, results, expressions);
    } catch (cl::Error e) {
      check_opencl_error(function, e);
    }
  }
};

/**
 * Deduces types for constructing \c results_cl object.
 * @tparam T_results types of results
 * @param results results that will be calculated in same kernel.
 */
template <typename... T_results>
results_cl<T_results...> results(T_results&&... results) {
  return results_cl<T_results...>(std::forward<T_results>(results)...);
}

}  // namespace math
}  // namespace stan

#endif
#endif
