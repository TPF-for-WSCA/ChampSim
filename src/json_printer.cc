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

#include <algorithm>
#include <utility>
#include <nlohmann/json.hpp>

#include "stats_printer.h"

void to_json(nlohmann::json& j, const O3_CPU::stats_type& stats)
{
  constexpr std::array types{branch_type::BRANCH_DIRECT_JUMP, branch_type::BRANCH_INDIRECT,      branch_type::BRANCH_CONDITIONAL,
                             branch_type::BRANCH_DIRECT_CALL, branch_type::BRANCH_INDIRECT_CALL, branch_type::BRANCH_RETURN};

  auto total_mispredictions = std::ceil(
      std::accumulate(std::begin(types), std::end(types), 0LL, [btm = stats.branch_type_misses](auto acc, auto next) { return acc + btm.value_or(next, 0); }));

  std::map<std::string, std::size_t> mpki{}, btb_accesses{}, btb_misses{};
  for (auto type : types) {
    mpki.emplace(branch_type_names.at(champsim::to_underlying(type)), stats.branch_type_misses.value_or(type, 0));
    btb_accesses.emplace(branch_type_names.at(champsim::to_underlying(type)), stats.branch_btb_accesses.value_or(type, 0));
    btb_misses.emplace(branch_type_names.at(champsim::to_underlying(type)), stats.branch_btb_misses.value_or(type, 0));
  }

  j = nlohmann::json{{"fdip_fetch_blocks", stats.fdip_fetch_blocks},
                     {"fdip_predictions", stats.fdip_predictions},
                     {"fdip_ftq_full_cycles", stats.fdip_ftq_full_cycles},
                     {"fdip_redirects", stats.fdip_redirects},
                     {"fdip_discontinuities", stats.fdip_discontinuities},
                     {"btb_lookups", stats.btb_lookups},
                     {"btb_hits", stats.btb_hits},
                     {"branch_direction_misses", stats.branch_direction_misses},
                     {"branch_target_misses", stats.branch_target_misses},
                     {"branch_redirects", stats.branch_redirects},
                     {"branch_redirect_cycles", stats.branch_redirect_cycles},
                     {"fetch_recovery_cycles", stats.fetch_recovery_cycles},
                     {"frontend_empty_cycles", stats.frontend_empty_cycles},
                     {"rob_empty_cycles", stats.rob_empty_cycles},
                     {"wrong_path_fetched", stats.wrong_path_fetched},
                     {"wrong_path_squashed", stats.wrong_path_squashed},
                     {"wrong_path_unknown", stats.wrong_path_unknown},
                     {"wrong_path_inferred_loads", stats.wrong_path_inferred_loads},
                     {"wrong_path_inferred_arithmetic", stats.wrong_path_inferred_arithmetic},
                     {"wrong_path_inferred_stores", stats.wrong_path_inferred_stores},
                     {"wrong_path_dispatched", stats.wrong_path_dispatched},
                     {"wrong_path_executed", stats.wrong_path_executed},
                     {"wrong_path_loads_issued", stats.wrong_path_loads_issued},
                     {"wrong_path_stores_executed", stats.wrong_path_stores_executed},
                     {"instructions", stats.instrs()},
                     {"cycles", stats.cycles()},
                     {"Avg ROB occupancy at mispredict", std::ceil(stats.total_rob_occupancy_at_branch_mispredict) / std::ceil(total_mispredictions)},
                     {"mispredict", mpki}, {"btb accesses", btb_accesses}, {"btb misses", btb_misses}};
}

void to_json(nlohmann::json& j, const CACHE::stats_type& stats)
{
  using hits_value_type = typename decltype(stats.hits)::value_type;
  using misses_value_type = typename decltype(stats.misses)::value_type;
  using miss_merge_value_type = typename decltype(stats.miss_merge)::value_type;
  using partial_misses_value_type = typename decltype(stats.partial_misses)::value_type;
  using fill_value_type = typename decltype(stats.fill)::value_type;

  std::map<std::string, nlohmann::json> statsmap;
  statsmap.emplace("prefetch requested", stats.pf_requested);
  statsmap.emplace("prefetch issued", stats.pf_issued);
  statsmap.emplace("useful prefetch", stats.pf_useful);
  statsmap.emplace("useless prefetch", stats.pf_useless);

  uint64_t total_downstream_demands = stats.fill.total();
  for (std::size_t cpu = 0; cpu < NUM_CPUS; ++cpu)
    total_downstream_demands -= stats.fill.value_or(std::pair{access_type::PREFETCH, cpu}, fill_value_type{});

  statsmap.emplace("miss latency", std::ceil(stats.total_miss_latency_cycles) / std::ceil(total_downstream_demands));
  for (const auto type : {access_type::LOAD, access_type::RFO, access_type::PREFETCH, access_type::WRITE, access_type::TRANSLATION}) {
    std::vector<hits_value_type> hits;
    std::vector<misses_value_type> misses;
    std::vector<miss_merge_value_type> miss_merges;
    std::vector<partial_misses_value_type> partial_misses;

    for (std::size_t cpu = 0; cpu < NUM_CPUS; ++cpu) {
      hits.push_back(stats.hits.value_or(std::pair{type, cpu}, hits_value_type{}));
      misses.push_back(stats.misses.value_or(std::pair{type, cpu}, misses_value_type{}));
      miss_merges.push_back(stats.miss_merge.value_or(std::pair{type, cpu}, miss_merge_value_type{}));
      partial_misses.push_back(stats.partial_misses.value_or(std::pair{type, cpu}, partial_misses_value_type{}));
    }

    statsmap.emplace(access_type_names.at(champsim::to_underlying(type)),
                     nlohmann::json{{"hit", hits}, {"miss", misses}, {"partial_miss", partial_misses}, {"miss_merge", miss_merges}});
  }

  j = statsmap;
}

void to_json(nlohmann::json& j, const DRAM_CHANNEL::stats_type stats)
{
  j = nlohmann::json{{"RQ ROW_BUFFER_HIT", stats.RQ_ROW_BUFFER_HIT},
                     {"RQ ROW_BUFFER_MISS", stats.RQ_ROW_BUFFER_MISS},
                     {"WQ ROW_BUFFER_HIT", stats.WQ_ROW_BUFFER_HIT},
                     {"WQ ROW_BUFFER_MISS", stats.WQ_ROW_BUFFER_MISS},
                     {"AVG DBUS CONGESTED CYCLE", (std::ceil(stats.dbus_cycle_congested) / std::ceil(stats.dbus_count_congested))},
                     {"REFRESHES ISSUED", stats.refresh_cycles}};
}

namespace champsim
{
void to_json(nlohmann::json& j, const champsim::phase_stats stats)
{
  std::map<std::string, nlohmann::json> roi_stats;
  roi_stats.emplace("cores", stats.roi_cpu_stats);
  roi_stats.emplace("DRAM", stats.roi_dram_stats);
  for (auto x : stats.roi_cache_stats) {
    roi_stats.emplace(x.name, x);
  }

  std::map<std::string, nlohmann::json> sim_stats;
  sim_stats.emplace("cores", stats.sim_cpu_stats);
  sim_stats.emplace("DRAM", stats.sim_dram_stats);
  for (auto x : stats.sim_cache_stats) {
    sim_stats.emplace(x.name, x);
  }

  std::map<std::string, nlohmann::json> statsmap{{"name", stats.name}, {"traces", stats.trace_names}};
  statsmap.emplace("roi", roi_stats);
  statsmap.emplace("sim", sim_stats);
  j = statsmap;
}
} // namespace champsim

void champsim::json_printer::print(std::vector<phase_stats>& stats) { stream << nlohmann::json::array_t{std::begin(stats), std::end(stats)}; }
