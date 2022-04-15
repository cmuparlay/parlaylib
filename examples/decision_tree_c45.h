#include <limits>
#include <tuple>
#include <utility>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

// **************************************************************
// Parallel code for building decision trees.
// Parallel algorithm based on C4.5 decision trees:
//   J. Ross Quinlan
//   C4.5: Programs for Machine Learning
//   Morgan Kaufmann Publishers, 1993,
// which in turn is based on Quinlan's earlier work on ID3.
// Works with both discrete and continuous non-target
// features.  For continuous features it tries all binary cuts and
// picks best.  The target label must be discrete (i.e. no
// regression).
// All features (both continuous and discrete) can have a max integer
// value of 255 so they can fit in an unsigned char.
// **************************************************************

// Some type definitions
constexpr int max_value = 255;
using value = unsigned char;
using row = parlay::sequence<value>;
using rows = parlay::sequence<row>;

struct feature {
  bool discrete; // discrete (true) or continuous (false)
  int num;       // max value of feature
  row vals;      // the sequence of values for the feature
  feature(bool discrete, int num) : discrete(discrete), num(num) {}
  feature(bool d, int n, row v) : discrete(d), num(n), vals(v) {}
};
using features = parlay::sequence<feature>;

using namespace parlay;
double inf = std::numeric_limits<double>::infinity();

struct tree {
  bool is_leaf;
  int feature_index;
  int feature_cut;
  int best;
  size_t size;
  sequence<tree*> children;
  tree(int i, int c, int best, sequence<tree*> children)
      : is_leaf(false), best(best), feature_index(i), feature_cut(c), children(children) {
    size = reduce(delayed_map(children, [] (tree* t) {return t->size;}));}
  tree(int best) : is_leaf(true), best(best), size(1) {}
};

// allocate and initialize a leaf
tree* Leaf(int best) {
  if (best > max_value) abort();
  return new tree(best);
}

// allocate and initialize an internal node
tree* Internal(int i, int cut, int majority, sequence<tree*> children) {
  return new tree(i, cut, majority, children);
}

// Some helper functions
template <typename S1, typename S2>
auto delayed_zip(S1 const &a, S2 const &b) {
  return delayed_tabulate(a.size(), [&] (size_t i) {return std::pair(a[i],b[i]);});
}

template <typename S1>
auto all_equal(S1 const &a) {
  return (a.size() == 0) || (count(a, a[0]) == a.size());
}

template <typename S1>
auto majority(S1 const &a, size_t m) {
  auto x = histogram_by_index(a,m);
  return max_element(x) - x.begin();
}

// Entropy * size.
template <typename Seq>
double entropy(Seq a, int total) {
  return reduce(delayed_map(a, [=] (int l) {
    return (l > 0) ? -(l * log2(float(l)/total)) : 0.0;}));
}

// The conditional information of a (with labels) based on b (a
// continuous, although discrete variable).   It picks the cut of b
// that minimizes this information, returning the cut value and
// the conditional information at that value.
auto cond_info_continuous(feature const &a, feature const &b) {
  int num_buckets = a.num * b.num;
  size_t n = a.vals.size();
  auto sums = histogram_by_index(delayed_tabulate(n, [&] (size_t i) {
    return a.vals[i] + b.vals[i]*a.num;}), num_buckets);
  sequence<int> low_counts(a.num, 0);
  sequence<int> high_counts(a.num, 0);
  for (int i=0; i < b.num; i++)
    for (int j=0; j < a.num; j++) high_counts[j] += sums[a.num*i + j];
  double cur_e = inf;
  int cur_i = 0;
  int m = 0;
  for (int i=0; i < b.num-1; i++) {
    for (int j=0; j < a.num; j++) {
      low_counts[j] += sums[a.num*i + j];
      high_counts[j] -= sums[a.num*i + j];
      m += sums[a.num*i + j];
    }
    double e = entropy(low_counts, m) + entropy(high_counts, n - m);
    if (e < cur_e) {
      cur_e = e;
      cur_i = i+1;
    }
  }
  return std::pair(cur_e, cur_i);
}

// information content of s (i.e. entropy * size)
double info(row s, int num_vals) {
  size_t n = s.size();
  if (n == 0) return 0.0;
  auto x = histogram_by_index(s, num_vals);
  return entropy(x, n);
}

// info of a conditioned on b
double cond_info_discrete(feature const &a, feature const &b) {
  int num_buckets = a.num * b.num;
  size_t n = a.vals.size();
  // histogram counts for every a x b value combination
  auto sums = histogram_by_index(delayed_tabulate(n, [&] (size_t i) {
    return a.vals[i] + b.vals[i]*a.num;}), num_buckets);
  // now for each b value, calculate entropy of a values that match it.
  return reduce(tabulate(b.num, [&] (size_t i) {
    auto x = sums.cut(i*a.num,(i+1)*a.num);
    return entropy(x, reduce(x));}));
}

// Recursively build the decision tree.  A is a sequence of features,
// each with a sequence of values for each point.  A[0] is the target
// feature and the rest are features to possibly select to branch on.
// Picks the feature that minimizes the conditional information of
// A[0] based on that feature, then splits the data on that feature
// and recurses.  The non-target features can be continuous, in which
// case they are split at the value that minimizes the conditional
// information on A[0].
auto build_tree(features &A) {
  int num_features = A.size();
  int num_entries = A[0].vals.size();

  // Base case if only one entry or all targets are equal
  int majority_value = (num_entries == 0) ? -1 : majority(A[0].vals, A[0].num);
  if (num_entries < 2 || all_equal(A[0].vals))
    return Leaf(majority_value);

  // For each feature calculate the conditional information of the
  // target A[0] based on each feature A[i+1].  Returns a tuple of the
  // information, the feature id, and, if A[i+1] is cotinuous, the cut
  // point.
  using results = std::tuple<double,int,int>;
  auto costs = tabulate(num_features - 1, [&] (int i) {
    if (A[i+1].discrete) {
      return std::tuple(cond_info_discrete(A[0], A[i+1]), i+1, -1);
    } else {
      auto [info, cut] = cond_info_continuous(A[0], A[i+1]);
      return results(info, i+1, cut);
    }},1);

  // Now find the feature which gives minimum conditional information
  // for the target A[0].
  auto minf = [&] (results a, results b) {return (std::get<0>(a) < std::get<0>(b)) ? a : b;};
  auto min_m = binary_op(minf, std::tuple(inf, 0, 0));
  auto [best_info, best_i, cutx] = reduce(costs, min_m);
  auto cut = cutx;

  double label_info = info(A[0].vals,A[0].num);
  double threshold = log2(float(num_features));

  // second base case if best information for cuts is too high, then make a leaf
  if (label_info - best_info < threshold)
    return Leaf(majority_value);

    // otherwise split and recurse
  else {
    int m;
    row split_on;

    // if the best feature is a label, then split on all possible
    // labels, otherwise split on cut point.  In the second case there
    // will only be two children (below and above the cut).
    if (A[best_i].discrete) {
      m = A[best_i].num;
      split_on = A[best_i].vals;
    } else {
      m = 2;
      split_on =  map(A[best_i].vals, [&] (value x) -> value {return x >= cut;});
    }

    features F = map(A, [&] (feature a) {return feature(a.discrete, a.num);});
    sequence<features> B(m, F);

    // For each feature (including the target), we need to paritition
    // into child buckets.
    parallel_for (0, num_features, [&] (size_t j) {
      auto x = group_by_index(delayed_zip(split_on, A[j].vals), m);
      for (int i=0; i < m; i++) B[i][j].vals = std::move(x[i]);
    }, 1);

    auto children = map(B, [&] (features &a) {return build_tree(a);}, 1);
    return Internal(best_i - 1, cut, majority_value, children); //-1 since first is label
  }
}

// Classify a new row based on the decision tree T.
int classify_row(tree* T, row const&r) {
  if (T->is_leaf) {
    return T->best;
  } else if (T->feature_cut == -1) { // discrete partition

    // could be a feature value in the test data that did not appear in training data
    // in this case return the best
    if (!(r[T->feature_index] < T->children.size())) return T->best;

    // go to child based on feature value
    int val = classify_row(T->children[r[T->feature_index]], r);
    return (val == -1) ? T->best : val;
  } else {  // continuous cut

    // go to child based on whether below or at-above cut value
    int idx = (r[T->feature_index] < T->feature_cut) ? 0 : 1;
    int val = classify_row(T->children[idx], r);
    return (val == -1) ? T->best : val;
  }
}

// Clasify a set of rows based on tree
row classify(tree* T, rows const &Test) {
  return map(Test, [&] (row const& r) -> value {
    return classify_row(T, r);});
}
