/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <numeric>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

#include "stats_printer.h"
#include <fmt/core.h>
#include <fmt/ostream.h>

void champsim::plain_printer::print(O3_CPU::stats_type stats)
{
  constexpr std::array<std::pair<std::string_view, std::size_t>, 7> types{
      {std::pair{"BRANCH_DIRECT_JUMP", BRANCH_DIRECT_JUMP}, std::pair{"BRANCH_INDIRECT", BRANCH_INDIRECT}, std::pair{"BRANCH_CONDITIONAL", BRANCH_CONDITIONAL},
       std::pair{"BRANCH_DIRECT_CALL", BRANCH_DIRECT_CALL}, std::pair{"BRANCH_INDIRECT_CALL", BRANCH_INDIRECT_CALL}, std::pair{"BRANCH_RETURN", BRANCH_RETURN},
       std::pair{"BRANCH_OTHER", BRANCH_OTHER}}};

  auto total_branch = std::ceil(
      std::accumulate(std::begin(types), std::end(types), 0ll, [tbt = stats.total_branch_types](auto acc, auto next) { return acc + tbt[next.second]; }));
  auto total_mispredictions = std::ceil(
      std::accumulate(std::begin(types), std::end(types), 0ll, [btm = stats.branch_type_misses](auto acc, auto next) { return acc + btm[next.second]; }));

  fmt::print(stream, "\nREGION BTB REPLACEMENTS: {}\n", stats.region_btb_conflicts);
  if (stats.region_btb_conflicts) {
    fmt::print(stream, "\nAVG MAX POINTER REGIONS: {}\n", stats.max_region_pointer_sum / stats.region_btb_conflicts);
  } else {
    fmt::print(stream, "\nAVG MAX POINTER REGIONS: ---\n");
  }

  fmt::print(stream, "\nBACK TO BACK BRANCHES: {}\tUNIQUE BRANCHES: {}", stats.back_to_back_branches, stats.unique_aligned_branches);

  fmt::print(stream, "\n{} cumulative IPC: {:.4g} instructions: {} cycles: {}\n", stats.name, std::ceil(stats.instrs()) / std::ceil(stats.cycles()),
             stats.instrs(), stats.cycles());
  fmt::print(stream, "{} Branch Prediction Accuracy: {:.4g}% MPKI: {:.4g} Average ROB Occupancy at Mispredict: {:.4g}\n", stats.name,
             (100.0 * std::ceil(total_branch - total_mispredictions)) / total_branch, (1000.0 * total_mispredictions) / std::ceil(stats.instrs()),
             std::ceil(stats.total_rob_occupancy_at_branch_mispredict) / total_mispredictions);

  fmt::print(stream, "\nXXX REGION BTB POINTER COUNT SAMPLED:\n");
  for (auto it = stats.region_pointer_cycle_probe_stats.begin(); it != stats.region_pointer_cycle_probe_stats.end(); it++) {
    fmt::print(stream, "{}:\t{}\n", it->first, it->second);
  }
  fmt::print(stream, "XXX END REGION BTB POINTER COUNT SAMPLED\n\n");

  fmt::print(stream, "\nXXX REGION BTB POINTER COUNT MAX:\n");
  for (auto it = stats.region_pointer_max_stats.begin(); it != stats.region_pointer_max_stats.end(); it++) {
    fmt::print(stream, "{}:\t{}\n", it->first, it->second);
  }
  fmt::print(stream, "XXX END REGION BTB POINTER COUNT MAX\n\n");

  fmt::print(stream, "{} REGION BTB BIG REGIONS: {}\n", stats.name, stats.big_region_small_region_mapping.size());
  fmt::print(stream, "\tBIG_REGION_IDX:\tSUB_REGION_COUNT\n");
  uint64_t total_regions = 0;
  for (auto const& [big_region, tags] : stats.big_region_small_region_mapping) {
    fmt::print(stream, "\t{}:\t{}\n", big_region, tags.size());
    total_regions += tags.size();
  }

  fmt::print(stream, "{} REGIONS BY SIZE:\n", stats.name);
  for (uint64_t i = 0; i < 64; i++) {
    fmt::print(stream, "\t{}:\t{}\n", 64 - i, stats.max_regions_per_region_size[i]);
  }

  fmt::print(stream, "\nREGION SET IDX\tINSERTS\n");
  for (auto const& [idx, cnt] : stats.region_btb_inserts_per_set) {
    fmt::print(stream, "{}\t{}\n", idx, cnt);
  }

  // TODO: Fix whitespaces and fix ipc_data.py to extract the result with the fixed whitespace (if not done whitespace stripped)
  fmt::print(stream, "\nSQUASHED CYCLES:\tALIASING:{}\tBTB MISS: {}\tBP MISPREDICTIONS: {}\tTOTAL:{}\n", stats.aliasing_squashed_cycles, stats.btb_miss_squashed_cycles, stats.bp_mispredict_squashed_cycles, stats.total_squashed_cycles);
  fmt::print(stream, "FRONTEND SQUASHES:\tALIASING:{}\tTOTAL:{}\n", std::get<0>(stats.aliasing_squash_counts), std::get<0>(stats.squash_counts));
  fmt::print(stream, "FULL SQUASHES:\tALIASING:{}\tTOTAL:{}\n", std::get<1>(stats.aliasing_squash_counts), std::get<1>(stats.squash_counts));
  fmt::print(stream, "TOTAL SQUASHES:\tALIASING:{}\tTOTAL:{}\n", std::get<2>(stats.aliasing_squash_counts), std::get<2>(stats.squash_counts));

  fmt::print(stream, "\nPositive Aliasing: {}\nNegative Aliasing: {}\nNone-Branch Aliasing: {}\nTotal Aliasing: {}", stats.positive_aliasing,
             stats.negative_aliasing, stats.non_branch_btb_hits, stats.total_aliasing);
  fmt::print(stream, "\nALIASING NON BRANCHES: {}\tBRANCHES: {}", stats.aliasing_on_non_branch, stats.aliasing_on_branch);
  fmt::print(stream, "\n{} TOTAL REGIONS: {}\n", stats.name, total_regions);
  fmt::print(stream, "\nMAX BTB Regions: {}\nMIN BTB Regions: {}\n", stats.max_regions, stats.min_regions);

  fmt::print(stream, "\nALIASING BIT COUNTS:\n");
  for (uint64_t i = 0; i < 64; i++) {
    fmt::print(stream, "\t{}: \t{}\n", i, stats.aliasing_bit_counts[i]);
  }

  long double btb_tag_entropy = 0;
  long double switch_tag_entropy = 0;
  auto total_btb_updates = ((long double)stats.btb_updates);
  auto total_btb_static_updates = ((long double)stats.btb_static_updates);
  auto region_switching_frequency = 1.0f - (long double)stats.btb_region_switching_dynamic / total_btb_updates;
  fmt::print(stream, "\nREGION SWITCHING FREQUENCY: {}\n", region_switching_frequency);

  fmt::print(stream, "\n\nNumber of Regions Observed per Target Offset Way\n");
  for (auto const &[target_size, regions] : stats.regions_inserted_per_way) {
    fmt::print(stream, "{}:\t{}\n", target_size, regions.size());
  }
  long double prev_counter = 0, prev_switch = 0;
  std::vector<std::pair<long double, uint8_t>> tag_bit_order;
  std::vector<std::pair<long double, uint8_t>> switch_bit_order;
  tag_bit_order.reserve(64);
  switch_bit_order.reserve(64);
  for (size_t i = 0; i < 64; i++) {
    if (prev_counter == stats.btb_tag_entropy[i] and prev_switch == stats.btb_tag_switch_entropy[i]) {
      continue;
    }
    prev_counter =
        stats.btb_tag_entropy[i]; // This is to prevent blocks of equal bits to change the outcome: Blocks that all contain exactly the same information should
                                  // be ignored. We don't capture interleaving patterns here, but that can be added at a later stage
    prev_switch = stats.btb_tag_switch_entropy[i];
    long double percentage = stats.btb_tag_entropy[i] / total_btb_static_updates;
    long double switch_percentage = stats.btb_tag_switch_entropy[i] / total_btb_updates;
    if ((percentage == 1.0 or percentage == 0.0) and (switch_percentage == 0.0 or switch_percentage == 1.0))
      continue;
    auto local_switch_entropy =
        (switch_percentage) ? -1.0 * (switch_percentage * std::log2l(switch_percentage) + (1.0 - switch_percentage) * std::log2l(1.0 - switch_percentage)) : 0;
    auto local_entropy = (percentage) ? -1.0 * (percentage * std::log2l(percentage) + (1.0 - percentage) * std::log2l(1.0 - percentage)) : 0;
    if (std::isnan(local_entropy)) {
      std::cerr << "local entropy was nan -- double check this" << std::endl;
      continue;
    }
    tag_bit_order.push_back({local_entropy, i});
    switch_bit_order.push_back({local_switch_entropy, i});
  }
  std::sort(tag_bit_order.begin(), tag_bit_order.end(), [](auto& a, auto& b) { return a.first > b.first; });
  std::sort(switch_bit_order.begin(), switch_bit_order.end(), [](auto& a, auto& b) { return a.first > b.first; });
  std::vector<std::pair<long double, uint8_t>> filtered_tag_bit_order, filtered_switch_bit_order;
  prev_counter = 0.0;
  prev_switch = 0.0;
  // TODO: Double check if this filtering is correct
  for (size_t i = 0; i < tag_bit_order.size(); i++) {
    if (prev_counter != tag_bit_order[i].first) {
      filtered_tag_bit_order.push_back(tag_bit_order[i]);
      btb_tag_entropy += tag_bit_order[i].first;
    }
    if (prev_switch != switch_bit_order[i].first) {
      switch_tag_entropy += switch_bit_order[i].first;
      filtered_switch_bit_order.push_back(switch_bit_order[i]);
    }
    prev_counter = tag_bit_order[i].first;
    prev_switch = switch_bit_order[i].first;
  }

  fmt::print(stream, "\nBTB TAG Entropy: {}b\nBTB TAG Size: {}\nBTB TAG AVG Utilisation: {}\n", btb_tag_entropy, stats.btb_tag_size,
             btb_tag_entropy / (float)stats.btb_tag_size);
  fmt::print(stream, "\nBTB TAG SWITCH Entropy: {}b\n", switch_tag_entropy);

  fmt::print(stream, "BTB TAG Bits Sorted by Entropy\n");
  fmt::print(stream, "BTB TAG Bit IDX\tENTROPY\n");
  for (auto [entropy, bit_idx] : filtered_tag_bit_order) {
    fmt::print(stream, "{}\t{}\n", bit_idx, entropy);
  }

  fmt::print(stream, "BTB TAG Switched Bit IDX\tENTROPY\n");
  for (auto [entropy, bit_idx] : filtered_switch_bit_order) {
    fmt::print(stream, "{}\t{}\n", bit_idx, entropy);
  }

  fmt::print("XXX Total dynamic branch IPs: {}\n", stats.dynamic_branch_count);
  fmt::print("XXX Total dynamic 1 bits in branch IPs:\n");
  for (int j = 0; j < 64; j++) {
    fmt::print("{}:\t{}\n", j, (double)stats.dynamic_bit_counts[j] / (double)stats.dynamic_branch_count);
  }

  fmt::print("XXX Total static branch IPs: {}\n", stats.static_branch_count);
  fmt::print("XXX Total static 1 bits in branch IPs:\n");
  for (int j = 0; j < 64; j++) {
    fmt::print("{}:\t{}\n", j, (double)stats.static_bit_counts[j] / (double)stats.static_branch_count);
  }

  fmt::print("XXX Total dynamic branch IPs: {}\n", stats.dynamic_branch_count);
  fmt::print("XXX Total dynamic switched 1 bits in branch IPs:\n");
  for (int j = 0; j < 64; j++) {
    fmt::print("{}:\t{}\n", j, (double)stats.btb_tag_switch_entropy[j] / (double)stats.dynamic_branch_count);
  }

  fmt::print("XXX Total dynamic BTB lookup IPs: {}\n", stats.dynamic_btb_lookup_count);
  fmt::print("XXX Total dynamic switched 1 bits in lookup IPs:\n");
  for (int j = 0; j < 64; j++) {
    fmt::print("{}:\t{}\n", j, (double)stats.btb_tag_lookup_switch_entropy[j] / (double)stats.dynamic_btb_lookup_count);
  }

  fmt::print("XXX END BTB STATS\n");

  fmt::print("\n\nBTB\tREADS: {}\tHITS: {}\n", stats.btb_reads, stats.btb_hits);
  fmt::print("\nBTB REGIONS:\tMAX: {}\tMIN: {}\n", stats.max_regions, stats.min_regions);

  std::vector<double> mpkis;
  double total_mpki = 0.0;
  for (auto it = std::begin(stats.branch_type_misses); it != std::end(stats.branch_type_misses); it++) {
    double mpki = 1000.0 * std::ceil(*it) / std::ceil(stats.instrs());
    mpkis.push_back(mpki);
    total_mpki += mpki;
  }

  std::transform(std::begin(stats.branch_type_misses), std::end(stats.branch_type_misses), std::back_inserter(mpkis),
                 [instrs = stats.instrs()](auto x) { return 1000.0 * std::ceil(x) / std::ceil(instrs); });

  fmt::print(stream, "EAGER INVALIDATION MPKI: {:.10f}\n", 1000.0 * std::ceil(stats.btb_eager_invalidation_miss) / std::ceil(stats.instrs()));

  fmt::print(stream, "LAZY REPLACEMENT MPKI: {:.10f}\n", 1000.0 * std::ceil(stats.btb_unforced_useful_evictions) / std::ceil(stats.instrs()));

  fmt::print(stream, "EAGER INVALIDATION RECOVERY: {:.10f}\n", std::ceil(stats.btb_eager_invalidation_miss) / std::ceil(stats.btb_eager_invalidations));

  fmt::print(stream, "LAZY REPLACEMENT RATE: {:.10f}\n", std::ceil(stats.btb_unforced_useful_evictions) / std::ceil(stats.btb_total_evictions));

  fmt::print(stream, "TOTAL EAGER INVALIDATIONS: {}\n", stats.btb_eager_invalidations);
  fmt::print(stream, "TOTAL UNFORCED USEFUL EVICTIONS: {}\n", stats.btb_unforced_useful_evictions);

  fmt::print(stream, "UTB INDUCED MPKI: {:.3g}\n\n", 1000.0 * std::ceil(stats.utb_replacement_misses) / std::ceil(stats.instrs()));

  fmt::print(stream, "Way\t% of Insertions\n");
  const uint64_t total_insertions = std::accumulate(
    std::begin(stats.region_way_insertion_counts),
    std::end(stats.region_way_insertion_counts),
    0,
    [](uint64_t value, const std::map<int, int>::value_type& p) {
      return value + p.second;
    }
  );
  for (auto [way, count] : stats.region_way_insertion_counts) {
    fmt::print(stream, "{}\t{:.4f}\n", way, 100.0*std::ceil(count)/std::ceil(total_insertions));
  }

  fmt::print(stream, "BRANCH_MPKI: {:.3g}\n\n", total_mpki);

  fmt::print(stream, "Branch type MPKI\n");
  for (auto [str, idx] : types)
    fmt::print(stream, "{}: {:.3}\n", str, mpkis[idx]);
  fmt::print(stream, "Branch count: {}\n", total_branch);
  for (auto [str, idx] : types) {
    fmt::print(stream, "{}:\t{}\n", str, stats.total_branch_types[idx]);
  }
  fmt::print(stream, "\n");
  // fmt::print(stream, "90%\t95%\t99%\t99.5%\t100%\n");
  // for (auto [percentile90, percentile95, percentile99, percentile995, full] : stats.region_history) {
  //   fmt::print(stream, "{}\t{}\t{}\t{}\t{}\n", percentile90, percentile95, percentile99, percentile995, full);
  // }
  // fmt::print(stream, "\n");
}

void champsim::plain_printer::print(CACHE::stats_type stats)
{
  constexpr std::array<std::pair<std::string_view, std::size_t>, 5> types{
      {std::pair{"LOAD", champsim::to_underlying(access_type::LOAD)}, std::pair{"RFO", champsim::to_underlying(access_type::RFO)},
       std::pair{"PREFETCH", champsim::to_underlying(access_type::PREFETCH)}, std::pair{"WRITE", champsim::to_underlying(access_type::WRITE)},
       std::pair{"TRANSLATION", champsim::to_underlying(access_type::TRANSLATION)}}};

  for (std::size_t cpu = 0; cpu < NUM_CPUS; ++cpu) {
    uint64_t TOTAL_HIT = 0, TOTAL_MISS = 0;
    for (const auto& type : types) {
      TOTAL_HIT += stats.hits.at(type.second).at(cpu);
      TOTAL_MISS += stats.misses.at(type.second).at(cpu);
    }

    fmt::print(stream, "{} TOTAL        ACCESS: {:10d} HIT: {:10d} MISS: {:10d}\n", stats.name, TOTAL_HIT + TOTAL_MISS, TOTAL_HIT, TOTAL_MISS);
    for (const auto& type : types) {
      fmt::print(stream, "{} {:<12s} ACCESS: {:10d} HIT: {:10d} MISS: {:10d}\n", stats.name, type.first,
                 stats.hits[type.second][cpu] + stats.misses[type.second][cpu], stats.hits[type.second][cpu], stats.misses[type.second][cpu]);
    }

    fmt::print(stream, "{} PREFETCH REQUESTED: {:10} ISSUED: {:10} USEFUL: {:10} USELESS: {:10}\n", stats.name, stats.pf_requested, stats.pf_issued,
               stats.pf_useful, stats.pf_useless);

    fmt::print(stream, "{} AVERAGE MISS LATENCY: {:.4g} cycles\n", stats.name, stats.avg_miss_latency);
  }
}

void champsim::plain_printer::print(DRAM_CHANNEL::stats_type stats)
{
  fmt::print(stream, "\n{} RQ ROW_BUFFER_HIT: {:10}\n  ROW_BUFFER_MISS: {:10}\n", stats.name, stats.RQ_ROW_BUFFER_HIT, stats.RQ_ROW_BUFFER_MISS);
  if (stats.dbus_count_congested > 0)
    fmt::print(stream, " AVG DBUS CONGESTED CYCLE: {:.4g}\n", std::ceil(stats.dbus_cycle_congested) / std::ceil(stats.dbus_count_congested));
  else
    fmt::print(stream, " AVG DBUS CONGESTED CYCLE: -\n");
  fmt::print(stream, "WQ ROW_BUFFER_HIT: {:10}\n  ROW_BUFFER_MISS: {:10}\n  FULL: {:10}\n", stats.name, stats.WQ_ROW_BUFFER_HIT, stats.WQ_ROW_BUFFER_MISS,
             stats.WQ_FULL);
}

void champsim::plain_printer::print(champsim::phase_stats& stats)
{
  fmt::print(stream, "=== {} ===\n", stats.name);

  int i = 0;
  for (auto tn : stats.trace_names)
    fmt::print(stream, "CPU {} runs {}", i++, tn);

  if (NUM_CPUS > 1) {
    fmt::print(stream, "\nTotal Simulation Statistics (not including warmup)\n");

    for (const auto& stat : stats.sim_cpu_stats)
      print(stat);

    for (const auto& stat : stats.sim_cache_stats)
      print(stat);
  }

  fmt::print(stream, "\nRegion of Interest Statistics\n");

  for (const auto& stat : stats.roi_cpu_stats)
    print(stat);

  for (const auto& stat : stats.roi_cache_stats)
    print(stat);

  fmt::print(stream, "\nDRAM Statistics\n");
  for (const auto& stat : stats.roi_dram_stats)
    print(stat);
}

void champsim::plain_printer::print(std::vector<phase_stats>& stats)
{
  for (auto p : stats)
    print(p);
}
