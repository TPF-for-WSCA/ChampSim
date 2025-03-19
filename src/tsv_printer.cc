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

#include "stats_printer.h"

void champsim::tsv_printer::to_tsv(const O3_CPU::stats_type stats)
{
  stream << "branch IPs" << std::endl;
  for (auto branch_ip : stats.branch_ip_set) {
    stream << branch_ip << std::endl;
  }
}

void champsim::tsv_printer::to_tsv(const CACHE::stats_type stats) {}

void champsim::tsv_printer::to_tsv(const DRAM_CHANNEL::stats_type stats) {}

void champsim::tsv_printer::print(std::vector<phase_stats>& stats)
{
  for (auto& stat : stats) {
    stream << stat.name << std::endl;
    stream << "Core" << std::endl;
    for (auto core : stat.sim_cpu_stats)
      to_tsv(core);
    stream << "Cache" << std::endl;
    for (auto cache : stat.sim_cache_stats)
      to_tsv(cache);

    stream << "DRAM" << std::endl;
    for (auto dram : stat.sim_dram_stats)
      to_tsv(dram);
  }
}
