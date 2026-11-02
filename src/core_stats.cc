#include "core_stats.h"

cpu_stats operator-(cpu_stats lhs, cpu_stats rhs)
{
  lhs.begin_instrs -= rhs.begin_instrs;
  lhs.begin_cycles -= rhs.begin_cycles;
  lhs.end_instrs -= rhs.end_instrs;
  lhs.end_cycles -= rhs.end_cycles;
  lhs.total_rob_occupancy_at_branch_mispredict -= rhs.total_rob_occupancy_at_branch_mispredict;

  lhs.wrong_path_fetched -= rhs.wrong_path_fetched;
  lhs.wrong_path_squashed -= rhs.wrong_path_squashed;
  lhs.wrong_path_unknown -= rhs.wrong_path_unknown;
  lhs.wrong_path_inferred_loads -= rhs.wrong_path_inferred_loads;
  lhs.wrong_path_inferred_arithmetic -= rhs.wrong_path_inferred_arithmetic;
  lhs.wrong_path_inferred_stores -= rhs.wrong_path_inferred_stores;
  lhs.wrong_path_dispatched -= rhs.wrong_path_dispatched;
  lhs.wrong_path_executed -= rhs.wrong_path_executed;
  lhs.wrong_path_loads_issued -= rhs.wrong_path_loads_issued;
  lhs.wrong_path_stores_executed -= rhs.wrong_path_stores_executed;

  lhs.fdip_fetch_blocks -= rhs.fdip_fetch_blocks;
  lhs.fdip_predictions -= rhs.fdip_predictions;
  lhs.fdip_ftq_full_cycles -= rhs.fdip_ftq_full_cycles;
  lhs.fdip_redirects -= rhs.fdip_redirects;
  lhs.fdip_discontinuities -= rhs.fdip_discontinuities;
  lhs.btb_lookups -= rhs.btb_lookups;
  lhs.btb_hits -= rhs.btb_hits;
  lhs.branch_direction_misses -= rhs.branch_direction_misses;
  lhs.branch_target_misses -= rhs.branch_target_misses;
  lhs.branch_redirects -= rhs.branch_redirects;
  lhs.branch_redirect_cycles -= rhs.branch_redirect_cycles;
  lhs.fetch_recovery_cycles -= rhs.fetch_recovery_cycles;
  lhs.frontend_empty_cycles -= rhs.frontend_empty_cycles;
  lhs.rob_empty_cycles -= rhs.rob_empty_cycles;

  lhs.total_branch_types -= rhs.total_branch_types;
  lhs.branch_type_misses -= rhs.branch_type_misses;
  lhs.branch_btb_accesses -= rhs.branch_btb_accesses;
  lhs.branch_btb_misses -= rhs.branch_btb_misses;

  return lhs;
}
