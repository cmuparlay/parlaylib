#include <iostream>
#include <string>

#include <parlay/primitives.h>
#include <parlay/sequence.h>
#include <parlay/internal/get_time.h>

#include "decision_tree_c45.h"
#include "helper/graph_utils.h"

// **************************************************************
// Driver
// **************************************************************

void report_correct(row result, row labels) {
  size_t n = result.size();
  if (n != labels.size()) {
    std::cout << "size mismatch of results and labels" << std::endl;
    return;
  }
  size_t num_correct = parlay::reduce(parlay::tabulate(n, [&] (size_t i) {
    return (result[i] == labels[i]) ? 1 : 0;}));
  float percent_correct = (100.0 * num_correct)/n;
  std::cout << num_correct << " correct out of " << n
            << ", " << percent_correct << " percent" << std::endl;
}

// Read from a csv file with one entry (sample) per line.  First line
// spefies the types of each column (feature), either 'd' (discrete)
// or 'i' (continuous).  Last column is the label and must be
// discrete.
auto read_data(std::string filename) {
  int num_buckets = 64;
  auto is_line = [] (char c) -> bool { return c == '\r' || c == '\n'|| c == 0;};
  auto is_item = [] (char c) -> bool { return c == ',';};
  auto str = parlay::file_map(filename);
  size_t j = find_if(str,is_line) - str.begin();
  auto head = parlay::make_slice(str).cut(0,j);
  auto rest = parlay::make_slice(str).cut(j+1, str.end()-str.begin());
  auto types = parlay::map(parlay::tokens(head, is_item), [] (auto str) {return str[0];});

  auto process_line = [&] (auto line) {
    auto to_int = [&] (auto x) -> value {
      int v = parlay::chars_to_int(parlay::to_sequence(x));
      if (v < 0 || v > max_value) {
        std::cout << "entry out range: value = " << v << std::endl;
        return 0;
      }
      return v;
    };
    return parlay::map_tokens(line, to_int, is_item);};

  return std::pair(types, parlay::map_tokens(rest, process_line, is_line));
}

// transposes the matrix making a sequence of features
// also places the target label up front (taken from the back)
auto rows_to_features(sequence<char> types, rows const &A) {
  int num_features = types.size();
  size_t num_rows = A.size();

  auto get_feature = [&] (size_t io) {
    int i = (io == 0) ? num_features - 1 : io - 1; // put label at front
    bool is_discrete = (types[i] == 'd');
    auto vals = parlay::tabulate(num_rows, [&] (size_t j) -> value {return A[j][i];});
    int maxv = parlay::reduce(vals, parlay::maximum<value>());
    return feature(is_discrete, maxv+1, vals);
  };

  return parlay::tabulate(num_features, get_feature);
}

auto read_features(std::string filename) {
  auto [types, rows] = read_data(filename);
  long num_train = rows.size()*.8;
  auto train_rows = parlay::to_sequence(rows.cut(0,num_train));
  auto test_rows = parlay::to_sequence(rows.cut(num_train,rows.size()));
  auto test_labels = parlay::map(test_rows, [&] (auto& row) {
    return row[row.size()-1]; });
  return std::tuple(rows_to_features(types, train_rows), test_rows, test_labels);
}

int main(int argc, char* argv[]) {
  auto usage = "Usage: decision_tree_c45 <filename>";
  if (argc != 2) std::cout << usage << std::endl;
  else {
    std::string filename = argv[1];
    auto [train_data, test_data, test_labels] = read_features(filename);
    parlay::internal::timer t("Time");
    tree* T;
    parlay::sequence<value> predicted_labels;
    for (int i=0; i < 5; i++) {
      T = build_tree(train_data);
      t.next("decision_tree_c45, build    80%");
      predicted_labels = classify(T, test_data);
      t.next("decision_tree_c45, classify 20%");
    }
    report_correct(predicted_labels, test_labels);
  }
}
