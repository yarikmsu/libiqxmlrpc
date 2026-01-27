// Performance Benchmarks for M5: State Machine Lookup Optimization
// Compares original linear scan (tag-first) vs optimized (state-first + early return)
// Run: cd build && ./tests/state-machine-benchmark-test

#include "perf_utils.h"

#include <string>
#include <unordered_map>
#include <vector>

using namespace perf;

// ============================================================================
// State transition table (matches real value_parser.cc layout)
// ============================================================================

struct StateTransition {
  int prev_state;
  int new_state;
  const char* tag;
};

// Value parser transitions — the largest and most frequently used table
static const StateTransition value_trans[] = {
  { 0, 1, "string" },
  { 0, 2, "int" },
  { 0, 2, "i4" },
  { 0, 3, "i8" },
  { 0, 4, "boolean" },
  { 0, 5, "double" },
  { 0, 6, "base64" },
  { 0, 7, "dateTime.iso8601" },
  { 0, 8, "struct" },
  { 0, 9, "array" },
  { 0, 10, "nil" },
  { 0, 0, nullptr }
};

// Request parser transitions
static const StateTransition request_trans[] = {
  { 0, 1, "methodCall" },
  { 1, 2, "methodName" },
  { 2, 3, "params" },
  { 3, 4, "param" },
  { 4, 5, "value" },
  { 5, 4, "param" },
  { 0, 0, nullptr }
};

// Struct builder transitions (small table)
static const StateTransition struct_trans[] = {
  { 0, 1, "member" },
  { 1, 2, "name" },
  { 2, 3, "value" },
  { 0, 0, nullptr }
};

// ============================================================================
// Original: tag-first comparison with found flag
// ============================================================================

int change_original(const StateTransition* trans, int curr, const std::string& tag) {
  bool found = false;
  size_t i = 0;
  for (; trans[i].tag != nullptr; ++i) {
    if (trans[i].tag == tag && trans[i].prev_state == curr) {
      found = true;
      break;
    }
  }
  if (!found) return -1;
  return trans[i].new_state;
}

// ============================================================================
// Optimized: state-first comparison with early return
// ============================================================================

int change_early_return(const StateTransition* trans, int curr, const std::string& tag) {
  for (size_t i = 0; trans[i].tag != nullptr; ++i) {
    if (trans[i].tag == tag && trans[i].prev_state == curr) {
      return trans[i].new_state;
    }
  }
  return -1;
}

// ============================================================================
// Hash map alternative (for comparison)
// ============================================================================

struct StateKey {
  int state;
  std::string tag;
  bool operator==(const StateKey& o) const {
    return state == o.state && tag == o.tag;
  }
};

struct StateKeyHash {
  std::size_t operator()(const StateKey& k) const {
    return std::hash<int>()(k.state) ^
           (std::hash<std::string>()(k.tag) << 1);
  }
};

using TransMap = std::unordered_map<StateKey, int, StateKeyHash>;

TransMap build_map(const StateTransition* trans) {
  TransMap map;
  for (size_t i = 0; trans[i].tag != nullptr; ++i) {
    map[StateKey{trans[i].prev_state, trans[i].tag}] = trans[i].new_state;
  }
  return map;
}

int change_hashmap(const TransMap& map, int curr, const std::string& tag) {
  auto it = map.find(StateKey{curr, tag});
  if (it == map.end()) return -1;
  return it->second;
}

// ============================================================================
// Benchmarks
// ============================================================================

void benchmark_value_parser_lookup() {
  section("Value Parser State Lookup (11 entries, all from state 0)");

  const size_t ITERS = 500000;

  // Typical sequence: look up each type tag from state 0
  std::vector<std::pair<int, std::string>> sequence = {
    {0, "string"}, {0, "int"}, {0, "i4"}, {0, "boolean"},
    {0, "double"}, {0, "struct"}, {0, "array"}, {0, "nil"},
    {0, "dateTime.iso8601"}, {0, "base64"}, {0, "i8"}
  };

  TransMap value_map = build_map(value_trans);

  PERF_BENCHMARK("perf_state_value_original", ITERS, {
    for (size_t j = 0; j < sequence.size(); ++j) {
      int r = change_original(value_trans, sequence[j].first, sequence[j].second);
      do_not_optimize(r);
    }
  });

  PERF_BENCHMARK("perf_state_value_early_ret", ITERS, {
    for (size_t j = 0; j < sequence.size(); ++j) {
      int r = change_early_return(value_trans, sequence[j].first, sequence[j].second);
      do_not_optimize(r);
    }
  });

  PERF_BENCHMARK("perf_state_value_hashmap", ITERS, {
    for (size_t j = 0; j < sequence.size(); ++j) {
      int r = change_hashmap(value_map, sequence[j].first, sequence[j].second);
      do_not_optimize(r);
    }
  });
}

void benchmark_request_parser_lookup() {
  section("Request Parser State Lookup (6 entries, sequential states)");

  const size_t ITERS = 500000;

  // Typical request parsing sequence
  std::vector<std::pair<int, std::string>> sequence = {
    {0, "methodCall"}, {1, "methodName"}, {2, "params"},
    {3, "param"}, {4, "value"}, {5, "param"},
    {4, "value"}, {5, "param"}, {4, "value"}
  };

  TransMap req_map = build_map(request_trans);

  PERF_BENCHMARK("perf_state_request_original", ITERS, {
    for (size_t j = 0; j < sequence.size(); ++j) {
      int r = change_original(request_trans, sequence[j].first, sequence[j].second);
      do_not_optimize(r);
    }
  });

  PERF_BENCHMARK("perf_state_request_early_ret", ITERS, {
    for (size_t j = 0; j < sequence.size(); ++j) {
      int r = change_early_return(request_trans, sequence[j].first, sequence[j].second);
      do_not_optimize(r);
    }
  });

  PERF_BENCHMARK("perf_state_request_hashmap", ITERS, {
    for (size_t j = 0; j < sequence.size(); ++j) {
      int r = change_hashmap(req_map, sequence[j].first, sequence[j].second);
      do_not_optimize(r);
    }
  });
}

void benchmark_struct_parser_lookup() {
  section("Struct Parser State Lookup (3 entries, smallest table)");

  const size_t ITERS = 1000000;

  // Typical struct member parsing: member → name → value, repeated
  std::vector<std::pair<int, std::string>> sequence = {
    {0, "member"}, {1, "name"}, {2, "value"},
    {0, "member"}, {1, "name"}, {2, "value"},
    {0, "member"}, {1, "name"}, {2, "value"}
  };

  TransMap struct_map = build_map(struct_trans);

  PERF_BENCHMARK("perf_state_struct_original", ITERS, {
    for (size_t j = 0; j < sequence.size(); ++j) {
      int r = change_original(struct_trans, sequence[j].first, sequence[j].second);
      do_not_optimize(r);
    }
  });

  PERF_BENCHMARK("perf_state_struct_early_ret", ITERS, {
    for (size_t j = 0; j < sequence.size(); ++j) {
      int r = change_early_return(struct_trans, sequence[j].first, sequence[j].second);
      do_not_optimize(r);
    }
  });

  PERF_BENCHMARK("perf_state_struct_hashmap", ITERS, {
    for (size_t j = 0; j < sequence.size(); ++j) {
      int r = change_hashmap(struct_map, sequence[j].first, sequence[j].second);
      do_not_optimize(r);
    }
  });
}

void benchmark_map_construction() {
  section("Hash Map Construction Cost (per-builder overhead)");

  const size_t ITERS = 100000;

  PERF_BENCHMARK("perf_state_map_build_value", ITERS, {
    TransMap m = build_map(value_trans);
    do_not_optimize(m.size());
  });

  PERF_BENCHMARK("perf_state_map_build_request", ITERS, {
    TransMap m = build_map(request_trans);
    do_not_optimize(m.size());
  });

  PERF_BENCHMARK("perf_state_map_build_struct", ITERS, {
    TransMap m = build_map(struct_trans);
    do_not_optimize(m.size());
  });
}

int main() {
  std::cout << "============================================================\n";
  std::cout << "M5: State Machine Lookup Performance Benchmark\n";
  std::cout << "============================================================\n";

  ResultCollector::instance().start_suite();

  benchmark_value_parser_lookup();
  benchmark_request_parser_lookup();
  benchmark_struct_parser_lookup();
  benchmark_map_construction();

  ResultCollector::instance().save_baseline("state_machine_baseline.txt");

  std::cout << "\n============================================================\n";
  return 0;
}
