#ifndef CORE_STATS_H
#define CORE_STATS_H

#include <cstdint>
#include <string>

#include "event_counter.h"
#include "instruction.h"

struct cpu_stats {
  std::string name;
  long long begin_instrs = 0;
  long long begin_cycles = 0;
  long long end_instrs = 0;
  long long end_cycles = 0;
  uint64_t wrong_path_fetched = 0, wrong_path_squashed = 0, wrong_path_unknown = 0;
  uint64_t wrong_path_inferred_loads = 0, wrong_path_inferred_arithmetic = 0, wrong_path_inferred_stores = 0;
  uint64_t wrong_path_dispatched = 0, wrong_path_executed = 0, wrong_path_loads_issued = 0, wrong_path_stores_executed = 0;
  uint64_t total_rob_occupancy_at_branch_mispredict = 0;
  uint64_t fdip_fetch_blocks = 0, fdip_predictions = 0, fdip_ftq_full_cycles = 0, fdip_redirects = 0, fdip_discontinuities = 0;
  uint64_t btb_lookups = 0, btb_hits = 0;
  uint64_t branch_direction_misses = 0, branch_target_misses = 0;
  uint64_t branch_redirects = 0, branch_redirect_cycles = 0, fetch_recovery_cycles = 0;
  uint64_t frontend_empty_cycles = 0, rob_empty_cycles = 0;

  champsim::stats::event_counter<branch_type> total_branch_types = {};
  champsim::stats::event_counter<branch_type> branch_type_misses = {};
  champsim::stats::event_counter<branch_type> branch_btb_accesses = {}, branch_btb_misses = {};

  [[nodiscard]] auto instrs() const { return end_instrs - begin_instrs; }
  [[nodiscard]] auto cycles() const { return end_cycles - begin_cycles; }
};

cpu_stats operator-(cpu_stats lhs, cpu_stats rhs);

#endif
