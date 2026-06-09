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
#include <fstream>
#include <numeric>
#include <signal.h>
#include <string>
#include <vector>

#include "champsim.h"
#include "champsim_constants.h"
#include "core_inst.inc"
#include "phase_info.h"
#include "stats_printer.h"
#include "tracereader.h"
#include "vmem.h"
#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <fmt/ranges.h>

namespace champsim
{
std::vector<phase_stats> main(environment& env, std::vector<phase_info>& phases, std::vector<tracereader>& traces);
}

void handler(int nSignum, siginfo_t* si, void* vcontext) {
  /*std::cout << "Segmentation fault" << std::endl;
  
  ucontext_t* context = (ucontext_t*)vcontext;
  context->uc_mcontext.gregs[REG_RIP]++;*/
  exit(-1);
}

int main(int argc, char** argv)
{
  champsim::configured::generated_environment gen_environment{};

  CLI::App app{"A microarchitecture simulator for research and education"};

  struct sigaction action;
  memset(&action, 0, sizeof(struct sigaction));
  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = handler;
  sigaction(SIGSEGV, &action, NULL);

  bool knob_cloudsuite{false};
  uint64_t warmup_instructions = 0;
  uint64_t simulation_instructions = std::numeric_limits<uint64_t>::max();
  std::string json_file_name;
  std::string tsv_file_name;
  std::string btb_index_tag_hash_file_name;
  std::string context_switch_trace_name;
  std::vector<uint8_t> context_switch_cpus;
  std::vector<std::string> trace_names;

  auto set_heartbeat_callback = [&](auto) {
    for (O3_CPU& cpu : gen_environment.cpu_view())
      cpu.show_heartbeat = false;
  };

  auto set_intel_callback = [&](auto) {
    for (O3_CPU& cpu : gen_environment.cpu_view()) {
      cpu.intel = true;
      cpu.isa_shiftamount = 0;
    }
  };

  app.add_flag("-c,--cloudsuite", knob_cloudsuite, "Read all traces using the cloudsuite format");
  app.add_flag("--hide-heartbeat", set_heartbeat_callback, "Hide the heartbeat output");
  app.add_flag("--intel", set_intel_callback, "Enable x86 isa support");
  auto warmup_instr_option = app.add_option("-w,--warmup-instructions", warmup_instructions, "The number of instructions in the warmup phase");
  auto deprec_warmup_instr_option =
      app.add_option("--warmup_instructions", warmup_instructions, "[deprecated] use --warmup-instructions instead")->excludes(warmup_instr_option);
  auto sim_instr_option = app.add_option("-i,--simulation-instructions", simulation_instructions,
                                         "The number of instructions in the detailed phase. If not specified, run to the end of the trace.");
  auto deprec_sim_instr_option =
      app.add_option("--simulation_instructions", simulation_instructions, "[deprecated] use --simulation-instructions instead")->excludes(sim_instr_option);

  auto json_option =
      app.add_option("--json", json_file_name, "The name of the file to receive JSON output. If no name is specified, stdout will be used")->expected(0, 1);
  auto tsv_option =
      app.add_option("--tsv", tsv_file_name, "The name of the file to receive TSV output. If no name is specified, stdout will be used")->expected(0, 1);
  auto btb_index_tag_hash = app.add_option("--btb-tag-hash", btb_index_tag_hash_file_name,
                                           "The name of the file that contains the ordering of the address bits to be used for indexing and tagging")
                                ->expected(0, 1);

  app.add_option("--context-switch-trace", context_switch_trace_name,
                 "The path to the trace to switch to after warmup phase (simulates context switch)")->check(CLI::ExistingFile);
  app.add_option("--context-switch-cpus", context_switch_cpus,
                 "The CPUs that will switch traces (default: CPU 0). Space-separated list of CPU indices.");

  app.add_option("traces", trace_names, "The paths to the traces")->required()->expected(NUM_CPUS)->check(CLI::ExistingFile);

  CLI11_PARSE(app, argc, argv);

  if (btb_index_tag_hash->count() > 0 && !btb_index_tag_hash_file_name.empty()) {
    std::ifstream f;
    f.open(btb_index_tag_hash_file_name.c_str());
    if (!f.is_open())
      throw std::runtime_error("file not opened");

    // TODO: Rewrite tag and indexing function to use infromation
    int bit_idx;
    while (f >> bit_idx) {
      for (O3_CPU& cpu : gen_environment.cpu_view()) {
        cpu.btb_index_tag_hash.push_back(bit_idx);
      }
    }
  }
  const bool warmup_given = (warmup_instr_option->count() > 0) || (deprec_warmup_instr_option->count() > 0);
  const bool simulation_given = (sim_instr_option->count() > 0) || (deprec_sim_instr_option->count() > 0);

  if (deprec_warmup_instr_option->count() > 0)
    fmt::print("WARNING: option --warmup_instructions is deprecated. Use --warmup-instructions instead.\n");

  if (deprec_sim_instr_option->count() > 0)
    fmt::print("WARNING: option --simulation_instructions is deprecated. Use --simulation-instructions instead.\n");

  if (simulation_given && !warmup_given)
    warmup_instructions = simulation_instructions * 2 / 10;

  std::vector<champsim::tracereader> traces;
  std::transform(
      std::begin(trace_names), std::end(trace_names), std::back_inserter(traces),
      [knob_cloudsuite, repeat = simulation_given, i = uint8_t(0)](auto name) mutable { return get_tracereader(name, i++, knob_cloudsuite, repeat); });

  // Handle context-switch trace if provided
  const bool context_switch_enabled = !context_switch_trace_name.empty();
  std::size_t context_switch_trace_index = 0;
  auto phase_trace_names = trace_names;
  
  if (context_switch_enabled) {
    if (!warmup_given)
      throw std::runtime_error("Context-switch trace requires --warmup-instructions to be specified");
    
    // Add context-switch trace to traces vector
    context_switch_trace_index = std::size(traces);
    traces.push_back(get_tracereader(context_switch_trace_name, static_cast<uint8_t>(std::size(traces)), knob_cloudsuite, simulation_given));
    phase_trace_names.push_back(context_switch_trace_name);
    
    // Default to CPU 0 if no specific CPUs specified
    if (context_switch_cpus.empty())
      context_switch_cpus.push_back(0);
  }

  std::vector<champsim::phase_info> phases{
      {champsim::phase_info{"Warmup", true, warmup_instructions, std::vector<std::size_t>(std::size(trace_names), 0), phase_trace_names, false, {}, 0},
       champsim::phase_info{"Simulation", false, simulation_instructions, std::vector<std::size_t>(std::size(trace_names), 0), phase_trace_names, false, {}, 0}}};

  for (auto& p : phases)
    std::iota(std::begin(p.trace_index), std::end(p.trace_index), 0);

  // Configure context-switch for simulation phase
  if (context_switch_enabled) {
    phases.at(1).context_switch_enabled = true;
    phases.at(1).context_switch_cpus = context_switch_cpus;
    phases.at(1).context_switch_trace_index = context_switch_trace_index;
    
    // Set simulation phase trace_index to use context-switch trace for specified CPUs
    for (uint8_t cpu_id : context_switch_cpus) {
      if (cpu_id < std::size(phases.at(1).trace_index))
        phases.at(1).trace_index[cpu_id] = context_switch_trace_index;
    }
    
    std::string trace_list;
    for (size_t i = 0; i < trace_names.size(); ++i) {
      if (i > 0) trace_list += ", ";
      trace_list += trace_names[i];
    }
    fmt::print("*** Context-Switch Simulation Enabled ***\nWarmup traces: {}\n", trace_list);
    std::string cpu_list;
    for (size_t i = 0; i < context_switch_cpus.size(); ++i) {
      if (i > 0) cpu_list += ", ";
      cpu_list += std::to_string(context_switch_cpus[i]);
    }
    fmt::print("Context-switch trace: {}\nContext-switch CPUs: {}\n\n",
               context_switch_trace_name, cpu_list);
  }

  fmt::print("\n*** ChampSim Multicore Out-of-Order Simulator ***\nWarmup Instructions: {}\nSimulation Instructions: {}\nNumber of CPUs: {}\nPage size: {}\n\n",
             phases.at(0).length, phases.at(1).length, std::size(gen_environment.cpu_view()), PAGE_SIZE);

  auto phase_stats = champsim::main(gen_environment, phases, traces);

  fmt::print("\nChampSim completed all CPUs\n\n");

  champsim::plain_printer{std::cout}.print(phase_stats);

  for (CACHE& cache : gen_environment.cache_view())
    cache.impl_prefetcher_final_stats();

  for (CACHE& cache : gen_environment.cache_view())
    cache.impl_replacement_final_stats();

  if (tsv_option->count() > 0) {
    if (tsv_file_name.empty()) {
      champsim::tsv_printer{std::cout}.print(phase_stats);
    } else {
      std::ofstream tsv_file{tsv_file_name};
      champsim::tsv_printer{tsv_file}.print(phase_stats);
    }
  }

  if (json_option->count() > 0) {
    if (json_file_name.empty()) {
      champsim::json_printer{std::cout}.print(phase_stats);
    } else {
      std::ofstream json_file{json_file_name};
      champsim::json_printer{json_file}.print(phase_stats);
    }
  }

  return 0;
}
