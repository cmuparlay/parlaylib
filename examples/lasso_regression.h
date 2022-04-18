#include <cmath>
#include <iostream>

#include <parlay/primitives.h>
#include <parlay/delayed.h>
#include <parlay/sequence.h>

// **************************************************************
// Lasso (least absolute shrinkage and selection operator) regression.
// i.e., for the optimization problem:
//      \arg \min_x ||Ax-y|| + \lambda |x|_1
// which is least square minization with L1 regularization.
// The columns of A are features (coordinates), and the rows samples.
// The y's are known values for each sample.
// The algorithm uses coordinate descent.
// The parallelism is non-deterministic since columns (coordinates)
// that are running in parallel can modifify the same entry of Ax.
// **************************************************************

// defined types
using real = double;
using vector = parlay::sequence<real>;
struct non_zero {long idx; real value;};
using sparse_vector = parlay::sequence<non_zero>;
using sparse_matrix = parlay::sequence<sparse_vector>;
struct feature {
  real covariance;  // Covariane of a a column
  real Ay_i;   // (A^T y)_i
};
using features = parlay::sequence<feature>;

// A variable to prevent false sharing on Ax, i.e. store
// elements in Ax with a stride of 8 so each on own cache line.
constexpr int stride=8;

// three helper functions
template <typename Seq, typename F>
auto map_reduce(const Seq& s, const F& f) {
  return parlay::reduce(parlay::delayed::map(s, f));}

template <typename F>
auto tab_reduce(long n, const F& f) {
  return parlay::reduce(parlay::delayed_tabulate(n, f));}

template <typename F>
auto max_tab_reduce(long n, const F& f) {
  return parlay::reduce(parlay::tabulate(n, f, 100),
                        parlay::maximum<real>());}

// Pre calculate feature (column) covariants and A^T * y.
auto initialize_features(const sparse_matrix& AT, const vector& y) {
  auto initialize_feature = [&] (const sparse_vector&  col) {
    return feature{2 * map_reduce(col, [] (auto c) {return c.value * c.value;}),
                   2 * map_reduce(col, [&] (auto c) {return c.value * y[c.idx];})};
  };
  return parlay::map(AT, initialize_feature);
}

// To deal with no gradient of the absolute value at 0.
real soft_threshold(real _lambda, real shootDiff) {
  if (shootDiff > _lambda) return _lambda - shootDiff;
  if (shootDiff < -_lambda) return -_lambda - shootDiff ;
  else return 0;
}

// Process one column, i.e., the meat.
// Find local gradient at coordinate and update xi and Ax.
double shoot(vector& Ax, feature feat, real& xi,
             const sparse_vector& col, real lambda) {
  real oldvalue = xi;
  real AtAxj = 0.0;
  for (int i =0; i < col.size(); i++)
    AtAxj += col[i].value * Ax[stride*col[i].idx];
  real S_j = 2 * AtAxj - feat.covariance * oldvalue - feat.Ay_i;
  real newvalue = soft_threshold(lambda,S_j)/feat.covariance;
  real delta = (newvalue - oldvalue);
  if (delta != 0.0) {
    for (int i = 0; i < col.size(); i++)
      // there is a race condition here, affects result slightly
      Ax[stride*col[i].idx] += col[i].value * delta;
    xi = newvalue;
  }
  return std::abs(delta);
}

// Find such lambda that if used for regularization,
// optimum would have all weights zero.
real compute_max_lambda(const features& col) {
  return max_tab_reduce(col.size(),
                        [&] (long i) {return std::abs(col[i].Ay_i);});
}

// Calculate the objective function ||Ax-y|| + \lambda |x|_1
real objective(const vector& Ax, const vector& x, const vector& y,
               real lambda) {
  return (lambda * map_reduce(x, [&] (real v) {return std::abs(v);})
          + tab_reduce(y.size(), [&] (long i) {
    return (Ax[stride*i]-y[i])*(Ax[stride*i]-y[i]);}));
}

// Iterative solver
// AT is the tranpsose of A, organized as colums
void solve_lasso(const sparse_matrix& AT, const vector& y,
                 double lambda, double target_objective) {
  long nx = AT.size();
  long ny = y.size();
  vector Ax(stride*ny, 0.0);
  vector x(nx, 0.0);
  auto feature_consts = initialize_features(AT, y);

  // Number of steps on the regularization path.
  // each decreases lambda to the final specified lambda.
  int num_steps = 50;
  real lambda_max = compute_max_lambda(feature_consts);
  real lambda_min = lambda;
  real alpha = pow(lambda_max/lambda_min, 1.0/(1.0*num_steps));
  int step = num_steps;
  real delta_threshold = 2.5*1e-3;
  int counter = 0;
  int total = 0;

  do {
    counter++; total++;
    lambda = lambda_min * pow(alpha, step);

    // Gradient decent the columns loosely synchronously in parallel
    // with racy writes
    real max_change = max_tab_reduce(nx, [&] (long i) {
      return shoot(Ax, feature_consts[i], x[i], AT[i], lambda);});

    // Convergence check
    if (step > 0) {
      if (max_change <= (step + 1) * delta_threshold || counter > 100) {
        step--;	counter = 0;
      }
    } else { // Only use actual objective on last step.
      real obj = objective(Ax, x, y, lambda);
      if (obj<target_objective || total>500 || max_change < delta_threshold) {
        std::cout << "objective = " << obj << ", max_change = "
                  << max_change << ", steps = " << total  << std::endl;
        return;
      }
    }
  } while (step >= 0);
}
