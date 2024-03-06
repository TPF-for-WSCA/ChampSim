#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <getopt.h>
#include <iomanip>
#include <signal.h>
#include <string.h>
#include <vector>

#include "cache.h"
#include "champsim.h"
#include "champsim_constants.h"
#include "dram_controller.h"
#include "ooo_cpu.h"
#include "operable.h"
#include "tracereader.h"
#include "vmem.h"

uint8_t warmup_complete[NUM_CPUS] = {}, simulation_complete[NUM_CPUS] = {}, trace_ended[NUM_CPUS] = {}, all_warmup_complete = 0, all_simulation_complete = 0,
        MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS, knob_pintrace = 0, knob_cloudsuite = 0, knob_low_bandwidth = 0, knob_intel = 0, knob_stall_on_miss = 1,
        knob_stop_at_completion = 1, knob_collect_offsets = 0;
int8_t knob_ip_offset = 0;
bool knob_no_detail_stats = false;

uint64_t warmup_instructions = 1000000, simulation_instructions = 10000000;

std::string result_dir;

auto start_time = time(NULL);

// For backwards compatibility with older module source.
champsim::deprecated_clock_cycle current_core_cycle;

extern MEMORY_CONTROLLER DRAM;
extern VirtualMemory vmem;
extern std::array<O3_CPU*, NUM_CPUS> ooo_cpu;
extern std::array<CACHE*, NUM_CACHES> caches;
extern std::array<champsim::operable*, NUM_OPERABLES> operables;

std::vector<tracereader*> traces;

uint64_t champsim::deprecated_clock_cycle::operator[](std::size_t cpu_idx)
{
  static bool deprecate_printed = false;
  if (!deprecate_printed) {
    std::cout << "WARNING: The use of 'current_core_cycle[cpu]' is deprecated." << std::endl;
    std::cout << "WARNING: Use 'this->current_cycle' instead." << std::endl;
    deprecate_printed = true;
  }
  return ooo_cpu[cpu_idx]->current_cycle;
}

void record_roi_stats(uint32_t cpu, CACHE* cache)
{
  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    cache->roi_access[cpu][i] = cache->sim_access[cpu][i];
    cache->roi_hit[cpu][i] = cache->sim_hit[cpu][i];
    cache->roi_miss[cpu][i] = cache->sim_miss[cpu][i];
    cache->roi_partial_miss[cpu][i] = cache->sim_partial_miss[cpu][i];
    if (cache->buffer) {
      BUFFER_CACHE& bc = ((VCL_CACHE*)cache)->buffer_cache;
      cache->roi_access[cpu][i] += bc.sim_access[cpu][i];
      cache->roi_hit[cpu][i] += bc.sim_hit[cpu][i];
      cache->roi_miss[cpu][i] += bc.sim_miss[cpu][i];
      cache->roi_partial_miss[cpu][i] += bc.sim_partial_miss[cpu][i];
      bc.roi_access[cpu][i] = bc.sim_access[cpu][i];
      bc.roi_hit[cpu][i] += bc.sim_hit[cpu][i];
      bc.roi_miss[cpu][i] += bc.sim_miss[cpu][i];
      bc.roi_partial_miss[cpu][i] += bc.sim_partial_miss[cpu][i];
      assert(cache->roi_access[cpu][i] == cache->roi_hit[cpu][i] + cache->roi_miss[cpu][i]);
    }
  }
}

void print_block_stats(uint32_t cpu, CACHE* cache)
{
  cout << cache->NAME << " #CACHELINES WITH #HOLES: " << endl;
  std::ofstream csv_file;
  std::filesystem::path csv_file_path = result_dir;
  string filename = cache->NAME + "_num_cl_with_num_holes.tsv";
  csv_file_path /= filename;
  csv_file.open(csv_file_path, std::ios::out);
  cout << cache->NAME << "FILE SUCCESSFULLY OPENED" << endl;
  if (!csv_file) {
    std::cerr << "COULD NOT CREATE/OPEN FILE " << csv_file_path << std::endl;
    std::cerr << std::flush;
  } else {
    for (size_t i = 0; i < BLOCK_SIZE; i++) {
      if (csv_file) {
        csv_file << i << "\t" << cache->holecount_hist[cpu][i] << endl;
      }
      cout << setw(10) << i << setw(10) << cache->holecount_hist[cpu][i] << endl;
    }
    csv_file.close();
  }

  cout << cache->NAME << " #CACHELINES WITH HOLE SIZE: " << endl;
  csv_file_path.remove_filename();
  filename = cache->NAME + "_num_cl_with_hole_size.tsv";
  csv_file_path /= filename;
  csv_file.open(csv_file_path);
  if (!csv_file) {
    std::cerr << "COULD NOT CREATE/OPEN FILE " << csv_file_path << std::endl;
    std::cerr << std::flush;
  } else {
    for (size_t i = 0; i < BLOCK_SIZE; i++) {
      if (csv_file) {
        csv_file << i + 1 << "\t" << cache->holesize_hist[cpu][i] << endl;
      }
      cout << setw(10) << i << setw(10) << cache->holesize_hist[cpu][i] << endl;
    }
    csv_file.close();
  }

  cout << cache->NAME << " #CACHELINES WITH BLOCK SIZE: " << endl;
  csv_file_path.remove_filename();
  filename = cache->NAME + "_num_cl_with_block_size.tsv";
  csv_file_path /= filename;
  csv_file.open(csv_file_path);
  if (!csv_file) {
    std::cerr << "COULD NOT CREATE/OPEN FILE " << csv_file_path << std::endl;
    std::cerr << std::flush;
  }
  for (size_t i = 0; i < BLOCK_SIZE; i++) {
    if (csv_file) {
      csv_file << i + 1 << "\t" << cache->blsize_hist[cpu][i] << endl;
    }
    cout << setw(10) << i << setw(10) << cache->blsize_hist[cpu][i] << endl;
  }
  csv_file.close();

  cout << cache->NAME << " #CACHELINES WITH BLOCK SIZE SCALED BY ACCESS: " << endl;
  csv_file_path.remove_filename();
  filename = cache->NAME + "_num_cl_with_block_size_accesses_scaled.tsv";
  csv_file_path /= filename;
  csv_file.open(csv_file_path);
  if (!csv_file) {
    std::cerr << "COULD NOT CREATE/OPEN FILE " << csv_file_path << std::endl;
    std::cerr << std::flush;
  }
  for (size_t i = 0; i < BLOCK_SIZE; i++) {
    if (csv_file) {
      csv_file << i + 1 << "\t" << cache->blsize_hist_accesses[cpu][i] << endl;
    }
    cout << setw(10) << i << setw(10) << cache->blsize_hist_accesses[cpu][i] << endl;
  }
  csv_file.close();

  cout << cache->NAME << " #CACHELINES WITH #BYTES ACCESSED: " << endl;
  csv_file_path.remove_filename();
  filename = cache->NAME + "_num_cl_with_bytes_accessed.tsv";
  csv_file_path /= filename;
  csv_file.open(csv_file_path);
  if (!csv_file) {
    std::cerr << "COULD NOT CREATE/OPEN FILE " << csv_file_path << std::endl;
  }
  for (size_t i = 0; i < BLOCK_SIZE + 1; i++) {
    if (csv_file) {
      csv_file << i + 1 << "\t" << cache->cl_bytesaccessed_hist[cpu][i] << endl;
    }
    cout << setw(10) << i << setw(10) << cache->cl_bytesaccessed_hist[cpu][i] << endl;
  }
  csv_file.close();

  cout << cache->NAME << " #CACHELINES WITH #BYTES ACCESSED SCALED BY ACCESSES: " << endl;
  csv_file_path.remove_filename();
  filename = cache->NAME + "_num_cl_with_bytes_accessed_accesses_scaled.tsv";
  csv_file_path /= filename;
  csv_file.open(csv_file_path);
  if (!csv_file) {
    std::cerr << "COULD NOT CREATE/OPEN FILE " << csv_file_path << std::endl;
  }
  for (size_t i = 0; i < BLOCK_SIZE + 1; i++) {
    if (csv_file) {
      csv_file << i + 1 << "\t" << cache->cl_bytesaccessed_hist_accesses[cpu][i] << endl;
    }
    cout << setw(10) << i << setw(10) << cache->cl_bytesaccessed_hist_accesses[cpu][i] << endl;
  }
  csv_file.close();

  cout << cache->NAME << " #CACHELINES WITH BLOCK SIZE IGNORING HOLES: " << endl;
  csv_file_path.remove_filename();
  filename = cache->NAME + "_num_cl_with_first_to_last_size.tsv";
  csv_file_path /= filename;
  csv_file.open(csv_file_path);
  if (!csv_file) {
    std::cerr << "COULD NOT CREATE/OPEN FILE " << csv_file_path << std::endl;
  }
  for (size_t i = 0; i < BLOCK_SIZE; i++) {
    if (csv_file) {
      csv_file << i + 1 << "\t" << cache->blsize_ignore_holes_hist[cpu][i] << endl;
    }
    cout << setw(10) << i << setw(10) << cache->blsize_ignore_holes_hist[cpu][i] << endl;
  }
  csv_file.close();
}

void print_roi_stats(uint32_t cpu, CACHE* cache)
{
  uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0, TOTAL_PARTIAL_MISS = 0;

  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    TOTAL_ACCESS += cache->roi_access[cpu][i];
    TOTAL_HIT += cache->roi_hit[cpu][i];
    TOTAL_MISS += cache->roi_miss[cpu][i];
    TOTAL_PARTIAL_MISS += cache->roi_partial_miss[cpu][i];
    //     if (cache->buffer) {
    //       BUFFER_CACHE& bc = ((VCL_CACHE*)cache)->buffer_cache;
    //       cache->roi_access[cpu][i] += bc.sim_access[cpu][i];
    //       cache->roi_hit[cpu][i] += bc.sim_hit[cpu][i];
    //       cache->roi_miss[cpu][i] += bc.sim_miss[cpu][i];
    //       TOTAL_ACCESS += bc.sim_access[cpu][i];
    //       TOTAL_HIT += bc.sim_hit[cpu][i];
    //       TOTAL_MISS += bc.sim_miss[cpu][i];
    //       assert(cache->roi_access[cpu][i] == cache->roi_hit[cpu][i] + cache->roi_miss[cpu][i]);
    //     }
  }
  long double miss_ratio = 0;
  if (TOTAL_PARTIAL_MISS != 0 && TOTAL_MISS != 0)
    miss_ratio = ((long double)TOTAL_PARTIAL_MISS / (long double)TOTAL_MISS) * 100;

  if (TOTAL_ACCESS > 0) {
    cout << cache->NAME;
    cout << " TOTAL     ACCESS: " << setw(10) << TOTAL_ACCESS << "  HIT: " << setw(10) << TOTAL_HIT << "  MISS: " << setw(10) << TOTAL_MISS
         << "  PARTIAL MISS: " << setw(10) << TOTAL_PARTIAL_MISS << " ( " << fixed << setprecision(2) << miss_ratio << "%)" << endl;

    miss_ratio = 0;
    if (cache->roi_miss[cpu][0] != 0 && cache->roi_partial_miss[cpu][0] != 0)
      miss_ratio = ((long double)cache->roi_partial_miss[cpu][0] / (long double)cache->roi_miss[cpu][0]) * 100;
    cout << cache->NAME;
    cout << " LOAD      ACCESS: " << setw(10) << cache->roi_access[cpu][0] << "  HIT: " << setw(10) << cache->roi_hit[cpu][0] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][0] << "  PARTIAL MISS: " << setw(10) << cache->roi_partial_miss[cpu][0] << " ( " << fixed << setprecision(2) << miss_ratio
         << "%)" << endl;

    miss_ratio = 0;
    if (cache->roi_miss[cpu][1] != 0 && cache->roi_partial_miss[cpu][1] != 0)
      miss_ratio = ((long double)cache->roi_partial_miss[cpu][1] / (long double)cache->roi_miss[cpu][1]) * 100;
    cout << cache->NAME;
    cout << " RFO       ACCESS: " << setw(10) << cache->roi_access[cpu][1] << "  HIT: " << setw(10) << cache->roi_hit[cpu][1] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][1] << "  PARTIAL MISS: " << setw(10) << cache->roi_partial_miss[cpu][1] << " ( " << fixed << setprecision(2) << miss_ratio
         << "%)" << endl;

    miss_ratio = 0;
    if (cache->roi_miss[cpu][2] != 0 && cache->roi_partial_miss[cpu][2] != 0)
      miss_ratio = ((long double)cache->roi_partial_miss[cpu][2] / (long double)cache->roi_miss[cpu][2]) * 100;
    cout << cache->NAME;
    cout << " PREFETCH  ACCESS: " << setw(10) << cache->roi_access[cpu][2] << "  HIT: " << setw(10) << cache->roi_hit[cpu][2] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][2] << "  PARTIAL MISS: " << setw(10) << cache->roi_partial_miss[cpu][2] << " ( " << fixed << setprecision(2) << miss_ratio
         << "%)" << endl;

    miss_ratio = 0;
    if (cache->roi_miss[cpu][3] != 0 && cache->roi_partial_miss[cpu][3] != 0)
      miss_ratio = ((long double)cache->roi_partial_miss[cpu][3] / (long double)cache->roi_miss[cpu][3]) * 100;
    cout << cache->NAME;
    cout << " WRITEBACK ACCESS: " << setw(10) << cache->roi_access[cpu][3] << "  HIT: " << setw(10) << cache->roi_hit[cpu][3] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][3] << "  PARTIAL MISS: " << setw(10) << cache->roi_partial_miss[cpu][3] << " ( " << fixed << setprecision(2) << miss_ratio
         << "%)" << endl;

    miss_ratio = 0;
    if (cache->roi_miss[cpu][4] != 0 && cache->roi_partial_miss[cpu][4] != 0)
      miss_ratio = ((long double)cache->roi_partial_miss[cpu][4] / (long double)cache->roi_miss[cpu][4]) * 100;
    cout << cache->NAME;
    cout << " TRANSLATION ACCESS: " << setw(10) << cache->roi_access[cpu][4] << "  HIT: " << setw(10) << cache->roi_hit[cpu][4] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][4] << "  PARTIAL MISS: " << setw(10) << cache->roi_partial_miss[cpu][4] << " ( " << fixed << setprecision(2) << miss_ratio
         << "%)" << endl;

    cout << cache->NAME;
    cout << " PREFETCH  REQUESTED: " << setw(10) << cache->pf_requested << "  ISSUED: " << setw(10) << cache->pf_issued;
    cout << "  USEFUL: " << setw(10) << cache->pf_useful << "  USELESS: " << setw(10) << cache->pf_useless << " NOT ISSUED: " << setw(10)
         << cache->pf_not_issued << endl;

    cout << cache->NAME;
    cout << " EVICTIONS: " << setw(10) << cache->TOTAL_CACHELINES << "  USELESS: " << setw(10) << cache->USELESS_CACHELINE << endl;

    if (cache->buffer) {
      BUFFER_CACHE& bc = ((VCL_CACHE*)cache)->buffer_cache;
      miss_ratio = 0;
      if (bc.roi_miss[cpu][0] != 0 && bc.roi_partial_miss[cpu][0] != 0)
        miss_ratio = ((long double)bc.roi_partial_miss[cpu][0] / (long double)bc.roi_miss[cpu][0]) * 100;
      cout << bc.NAME;
      cout << " LOAD        ACCESS: " << setw(10) << bc.roi_access[cpu][0] << "  HIT: " << setw(10) << bc.roi_hit[cpu][0] << "  MISS: " << setw(10)
           << bc.roi_miss[cpu][0] << "  PARTIAL MISS: " << setw(10) << bc.roi_partial_miss[cpu][0] << " ( " << fixed << setprecision(2) << miss_ratio << "%)"
           << endl;

      miss_ratio = 0;
      if (bc.roi_miss[cpu][1] != 0 && bc.roi_partial_miss[cpu][1] != 0)
        miss_ratio = ((long double)bc.roi_partial_miss[cpu][1] / (long double)bc.roi_miss[cpu][1]) * 100;
      cout << bc.NAME;
      cout << " RFO         ACCESS: " << setw(10) << bc.roi_access[cpu][1] << "  HIT: " << setw(10) << bc.roi_hit[cpu][1] << "  MISS: " << setw(10)
           << bc.roi_miss[cpu][1] << "  PARTIAL MISS: " << setw(10) << bc.roi_partial_miss[cpu][1] << " ( " << fixed << setprecision(2) << miss_ratio << "%)"
           << endl;

      miss_ratio = 0;
      if (bc.roi_miss[cpu][2] != 0 && bc.roi_partial_miss[cpu][2] != 0)
        miss_ratio = ((long double)bc.roi_partial_miss[cpu][2] / (long double)bc.roi_miss[cpu][2]) * 100;
      cout << bc.NAME;
      cout << " PREFETCH    ACCESS: " << setw(10) << bc.roi_access[cpu][2] << "  HIT: " << setw(10) << bc.roi_hit[cpu][2] << "  MISS: " << setw(10)
           << bc.roi_miss[cpu][2] << "  PARTIAL MISS: " << setw(10) << bc.roi_partial_miss[cpu][2] << " ( " << fixed << setprecision(2) << miss_ratio << "%)"
           << endl;

      miss_ratio = 0;
      if (bc.roi_miss[cpu][3] != 0 && bc.roi_partial_miss[cpu][3] != 0)
        miss_ratio = ((long double)bc.roi_partial_miss[cpu][3] / (long double)bc.roi_miss[cpu][3]) * 100;
      cout << bc.NAME;
      cout << " WRITEBACK   ACCESS: " << setw(10) << bc.roi_access[cpu][3] << "  HIT: " << setw(10) << bc.roi_hit[cpu][3] << "  MISS: " << setw(10)
           << bc.roi_miss[cpu][3] << "  PARTIAL MISS: " << setw(10) << bc.roi_partial_miss[cpu][3] << " ( " << fixed << setprecision(2) << miss_ratio << "%)"
           << endl;

      miss_ratio = 0;
      if (bc.roi_miss[cpu][4] != 0 && bc.roi_partial_miss[cpu][4] != 0)
        miss_ratio = ((long double)bc.roi_partial_miss[cpu][4] / (long double)bc.roi_miss[cpu][4]) * 100;
      cout << bc.NAME;
      cout << " TRANSLATION ACCESS: " << setw(10) << bc.roi_access[cpu][4] << "  HIT: " << setw(10) << bc.roi_hit[cpu][4] << "  MISS: " << setw(10)
           << bc.roi_miss[cpu][4] << "  PARTIAL MISS: " << setw(10) << bc.roi_partial_miss[cpu][4] << " ( " << fixed << setprecision(2) << miss_ratio << "%)"
           << endl;

      miss_ratio = 0;
      if (bc.roi_miss[cpu][5] != 0 && bc.roi_partial_miss[cpu][5] != 0)
        miss_ratio = ((long double)bc.roi_partial_miss[cpu][5] / (long double)bc.roi_miss[cpu][5]) * 100;
      cout << bc.NAME;
      cout << " PREFETCH    ACCESS: " << setw(10) << bc.roi_access[cpu][5] << "  HIT: " << setw(10) << bc.roi_hit[cpu][5] << "  MISS: " << setw(10)
           << bc.roi_miss[cpu][5] << "  PARTIAL MISS: " << setw(10) << bc.roi_partial_miss[cpu][5] << " ( " << fixed << setprecision(2) << miss_ratio << "%)"
           << endl;

      cout << bc.NAME;
      cout << " EVICTIONS: " << setw(10) << bc.TOTAL_CACHELINES << "  USELESS: " << setw(10) << bc.USELESS_CACHELINE << endl;
    }

    cout << cache->NAME;
    uint64_t total_miss_latency = cache->total_miss_latency;
    if (cache->buffer) {
      total_miss_latency += ((VCL_CACHE*)cache)->buffer_cache.total_miss_latency;
    }
    cout << " AVERAGE MISS LATENCY: " << (1.0 * (total_miss_latency)) / TOTAL_MISS << " cycles" << endl;
    // cout << " AVERAGE MISS LATENCY: " <<
    // (cache->total_miss_latency)/TOTAL_MISS << " cycles " <<
    // cache->total_miss_latency << "/" << TOTAL_MISS<< endl;
    cout << cache->NAME;
    cout << " MPKI: " << (1000.0 * TOTAL_MISS) / (ooo_cpu[cpu]->num_retired - ooo_cpu[cpu]->begin_sim_instr) << endl;

    if (0 == cache->NAME.compare(cache->NAME.length() - 3, 3, "L1I")) {
      print_block_stats(cpu, cache);
      if (cache->buffer) {
        BUFFER_CACHE* bc = &((VCL_CACHE*)cache)->buffer_cache;
        cout << "+++ BUFFER BLOCK STATS +++" << endl;
        print_block_stats(cpu, bc);
      }
    }
  }
}

void print_sim_stats(uint32_t cpu, CACHE* cache)
{
  uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0, TOTAL_PARTIAL_MISS = 0;

  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    TOTAL_ACCESS += cache->sim_access[cpu][i];
    TOTAL_HIT += cache->sim_hit[cpu][i];
    TOTAL_MISS += cache->sim_miss[cpu][i];
    TOTAL_PARTIAL_MISS += cache->sim_partial_miss[cpu][i];
    if (cache->buffer) {
      BUFFER_CACHE& bc = ((VCL_CACHE*)cache)->buffer_cache;
      TOTAL_ACCESS += bc.sim_access[cpu][i];
      TOTAL_HIT += bc.sim_hit[cpu][i];
      TOTAL_MISS += bc.sim_miss[cpu][i];
    }
  }
  long double miss_ratio = 0;
  if (TOTAL_PARTIAL_MISS != 0 && TOTAL_MISS != 0)
    miss_ratio = ((long double)TOTAL_PARTIAL_MISS / (long double)TOTAL_MISS) * 100;

  if (TOTAL_ACCESS > 0) {
    cout << cache->NAME;
    cout << " TOTAL     ACCESS: " << setw(10) << TOTAL_ACCESS << "  HIT: " << setw(10) << TOTAL_HIT << "  MISS: " << setw(10) << TOTAL_MISS
         << "  PARTIAL MISS: " << setw(10) << TOTAL_PARTIAL_MISS << " ( " << fixed << setprecision(2) << miss_ratio << "%)" << endl;

    miss_ratio = 0;
    if (cache->sim_miss[cpu][0] != 0 && cache->sim_partial_miss[cpu][0] != 0)
      miss_ratio = ((long double)cache->sim_partial_miss[cpu][0] / (long double)cache->sim_miss[cpu][0]);
    cout << cache->NAME;
    cout << " LOAD      ACCESS: " << setw(10) << cache->sim_access[cpu][0] << "  HIT: " << setw(10) << cache->sim_hit[cpu][0] << "  MISS: " << setw(10)
         << cache->sim_miss[cpu][0] << "  PARTIAL MISS: " << setw(10) << cache->sim_partial_miss[cpu][0] << " ( " << fixed << setprecision(2) << miss_ratio
         << "%)" << endl;

    miss_ratio = 0;
    if (cache->sim_miss[cpu][1] != 0 && cache->sim_partial_miss[cpu][1] != 0)
      miss_ratio = ((long double)cache->sim_partial_miss[cpu][1] / (long double)cache->sim_miss[cpu][1]);
    cout << cache->NAME;
    cout << " RFO       ACCESS: " << setw(10) << cache->sim_access[cpu][1] << "  HIT: " << setw(10) << cache->sim_hit[cpu][1] << "  MISS: " << setw(10)
         << cache->sim_miss[cpu][1] << "  PARTIAL MISS: " << setw(10) << cache->sim_partial_miss[cpu][1] << " ( " << fixed << setprecision(2) << miss_ratio
         << "%)" << endl;

    miss_ratio = 0;
    if (cache->sim_miss[cpu][2] != 0 && cache->sim_partial_miss[cpu][2] != 0)
      miss_ratio = ((long double)cache->sim_partial_miss[cpu][2] / (long double)cache->sim_miss[cpu][2]);
    cout << cache->NAME;
    cout << " PREFETCH  ACCESS: " << setw(10) << cache->sim_access[cpu][2] << "  HIT: " << setw(10) << cache->sim_hit[cpu][2] << "  MISS: " << setw(10)
         << cache->sim_miss[cpu][2] << "  PARTIAL MISS: " << setw(10) << cache->sim_partial_miss[cpu][2] << " ( " << fixed << setprecision(2) << miss_ratio
         << "%)" << endl;

    miss_ratio = 0;
    if (cache->sim_miss[cpu][3] != 0 && cache->sim_partial_miss[cpu][3] != 0)
      miss_ratio = ((long double)cache->sim_partial_miss[cpu][3] / (long double)cache->sim_miss[cpu][3]);
    cout << cache->NAME;
    cout << " WRITEBACK ACCESS: " << setw(10) << cache->sim_access[cpu][3] << "  HIT: " << setw(10) << cache->sim_hit[cpu][3] << "  MISS: " << setw(10)
         << cache->sim_miss[cpu][3] << "  PARTIAL MISS: " << setw(10) << cache->sim_partial_miss[cpu][3] << " ( " << fixed << setprecision(2) << miss_ratio
         << "%)" << endl;

    if (0 == cache->NAME.compare(cache->NAME.length() - 3, 3, "L1I")) {
      print_block_stats(cpu, cache);
      if (cache->buffer) {
        BUFFER_CACHE* bc = &((VCL_CACHE*)cache)->buffer_cache;
        cout << "+++ BUFFER BLOCK STATS +++" << endl;
        print_block_stats(cpu, bc);
      }
    }

    // cout << cache->NAME << " #CACHELINES WITH #HOLES: " << endl;
    // std::ofstream csv_file;
    // std::filesystem::path csv_file_path = result_dir;
    // csv_file_path /= "num_cl_with_num_holes.tsv";
    // csv_file.open(csv_file_path, std::ios::out);
    // if (!csv_file) {
    //   std::cerr << "COULD NOT CREATE/OPEN FILE " << csv_file_path << std::endl;
    // }
    // for (size_t i = 0; i < BLOCK_SIZE; i++) {
    //   if (csv_file) {
    //     csv_file << i << "\t" << cache->holecount_hist[cpu][i] << endl;
    //   }
    //   cout << setw(10) << i << setw(10) << cache->holecount_hist[cpu][i] << endl;
    // }
    // csv_file.close();
    //
    // cout << cache->NAME << " #CACHELINES WITH HOLE SIZE: " << endl;
    // csv_file_path.remove_filename();
    // csv_file_path /= "num_cl_with_hole_size.tsv";
    // csv_file.open(csv_file_path);
    // if (!csv_file) {
    //  std::cerr << "COULD NOT CREATE/OPEN FILE " << csv_file_path << std::endl;
    //}
    // for (size_t i = 0; i < BLOCK_SIZE; i++) {
    //  if (csv_file) {
    //    csv_file << i << "\t" << cache->holesize_hist[cpu][i] << endl;
    //  }
    //  cout << setw(10) << i << setw(10) << cache->holesize_hist[cpu][i] << endl;
    //}
    // csv_file.close();
    //
    // cout << cache->NAME << " #CACHELINES WITH BLOCK SIZE: " << endl;
    // csv_file_path.remove_filename();
    // csv_file_path /= "num_cl_with_block_size.tsv";
    // csv_file.open(csv_file_path);
    // if (!csv_file) {
    //  std::cerr << "COULD NOT CREATE/OPEN FILE " << csv_file_path << std::endl;
    //}
    // for (size_t i = 0; i < BLOCK_SIZE; i++) {
    //  if (csv_file) {
    //    csv_file << i + 1 << "\t" << cache->blsize_hist[cpu][i] << endl;
    //  }
    //  cout << setw(10) << i << setw(10) << cache->blsize_hist[cpu][i] << endl;
    //}
    // csv_file.close();
    //
    // cout << cache->NAME << " #CACHELINES WITH #BYTES ACCESSED: " << endl;
    // csv_file_path.remove_filename();
    // csv_file_path /= "num_cl_with_bytes_accessed.tsv";
    // csv_file.open(csv_file_path);
    // if (!csv_file) {
    //  std::cerr << "COULD NOT CREATE/OPEN FILE " << csv_file_path << std::endl;
    //}
    // for (size_t i = 0; i < BLOCK_SIZE; i++) {
    //  if (csv_file) {
    //    csv_file << i + 1 << "\t" << cache->cl_bytesaccessed_hist[cpu][i] << endl;
    //  }
    //  cout << setw(10) << i << setw(10) << cache->cl_bytesaccessed_hist[cpu][i] << endl;
    //}
    // csv_file.close();
    //
    // cout << cache->NAME << " #CACHELINES WITH BLOCK SIZE IGNORING HOLES: " << endl;
    // csv_file_path.remove_filename();
    // csv_file_path /= "num_cl_with_first_to_last_size.tsv";
    // csv_file.open(csv_file_path);
    // if (!csv_file) {
    //  std::cerr << "COULD NOT CREATE/OPEN FILE " << csv_file_path << std::endl;
    //}
    // for (size_t i = 0; i < BLOCK_SIZE; i++) {
    //  if (csv_file) {
    //    csv_file << i + 1 << "\t" << cache->blsize_ignore_holes_hist[cpu][i] << endl;
    //  }
    //  cout << setw(10) << i << setw(10) << cache->blsize_ignore_holes_hist[cpu][i] << endl;
    //}
    // csv_file.close();
  }
}

void write_offsets(O3_CPU* cpu, int cpu_id)
{
  std::ofstream csv_file;
  std::filesystem::path csv_file_path = result_dir;
  string filename = "cpu" + std::to_string(cpu_id) + "_pc_offset_mapping.tsv";
  csv_file_path /= filename;
  csv_file.open(csv_file_path, std::ios::out);
  if (!csv_file) {
    std::cerr << "COULD NOT CREATE/OPEN FILE " << csv_file_path << std::endl;
    std::cerr << std::flush;
  } else {
    cout << csv_file_path << "FILE SUCCESSFULLY OPENED" << endl;
    for (auto elem : cpu->pc_offset_pairs) {
      csv_file << elem.first << "\t" << unsigned(elem.second) << endl;
    }
    csv_file.close();
  }
}

void print_branch_stats()
{
  for (uint32_t i = 0; i < NUM_CPUS; i++) {
    write_offsets(ooo_cpu[i], i);
    cout << endl << "CPU " << i << " Branch Prediction Accuracy: ";
    cout << (100.0 * (ooo_cpu[i]->num_branch - ooo_cpu[i]->branch_mispredictions)) / ooo_cpu[i]->num_branch;
    cout << "% MPKI: " << (1000.0 * ooo_cpu[i]->branch_mispredictions) / (ooo_cpu[i]->num_retired - warmup_instructions);
    cout << " Average ROB Occupancy at Mispredict: " << (1.0 * ooo_cpu[i]->total_rob_occupancy_at_branch_mispredict) / ooo_cpu[i]->branch_mispredictions
         << endl;
    cout << "BRANCH DISTANCE STATS" << endl;
    cout << "\tAVERAGE: " << ooo_cpu[i]->total_branch_distance / ooo_cpu[i]->branch_count << endl;
    cout << "\tTOTAL BRANCHES: " << ooo_cpu[i]->branch_count << std::endl;
    for (auto it = ooo_cpu[i]->branch_distance.begin(); it != ooo_cpu[i]->branch_distance.end(); it++) {
      cout << std::right << std::setw(9) << it->first << ": " << std::setw(6) << it->second << endl;
    }
    /*
    cout << "Branch types" << endl;
    cout << "NOT_BRANCH: " << ooo_cpu[i]->total_branch_types[0] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[0])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_DIRECT_JUMP: "
    << ooo_cpu[i]->total_branch_types[1] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[1])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_INDIRECT: " <<
    ooo_cpu[i]->total_branch_types[2] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[2])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_CONDITIONAL: "
    << ooo_cpu[i]->total_branch_types[3] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[3])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_DIRECT_CALL: "
    << ooo_cpu[i]->total_branch_types[4] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[4])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_INDIRECT_CALL:
    " << ooo_cpu[i]->total_branch_types[5] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[5])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_RETURN: " <<
    ooo_cpu[i]->total_branch_types[6] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[6])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_OTHER: " <<
    ooo_cpu[i]->total_branch_types[7] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[7])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl << endl;
    */

    cout << "Branch type MPKI" << endl;
    cout << "BRANCH_DIRECT_JUMP: " << (1000.0 * ooo_cpu[i]->branch_type_misses[1] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_INDIRECT: " << (1000.0 * ooo_cpu[i]->branch_type_misses[2] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_CONDITIONAL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[3] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_DIRECT_CALL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[4] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_INDIRECT_CALL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[5] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_RETURN: " << (1000.0 * ooo_cpu[i]->branch_type_misses[6] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl << endl;
  }
}

void print_dram_stats()
{
  uint64_t total_congested_cycle = 0;
  uint64_t total_congested_count = 0;

  std::cout << std::endl;
  std::cout << "DRAM Statistics" << std::endl;
  for (uint32_t i = 0; i < DRAM_CHANNELS; i++) {
    std::cout << " CHANNEL " << i << std::endl;

    auto& channel = DRAM.channels[i];
    std::cout << " RQ ROW_BUFFER_HIT: " << std::setw(10) << channel.RQ_ROW_BUFFER_HIT << " ";
    std::cout << " ROW_BUFFER_MISS: " << std::setw(10) << channel.RQ_ROW_BUFFER_MISS;
    std::cout << std::endl;

    std::cout << " DBUS AVG_CONGESTED_CYCLE: ";
    if (channel.dbus_count_congested)
      std::cout << std::setw(10) << ((double)channel.dbus_cycle_congested / channel.dbus_count_congested);
    else
      std::cout << "-";
    std::cout << std::endl;

    std::cout << " WQ ROW_BUFFER_HIT: " << std::setw(10) << channel.WQ_ROW_BUFFER_HIT << " ";
    std::cout << " ROW_BUFFER_MISS: " << std::setw(10) << channel.WQ_ROW_BUFFER_MISS << " ";
    std::cout << " FULL: " << std::setw(10) << channel.WQ_FULL;
    std::cout << std::endl;

    std::cout << std::endl;

    total_congested_cycle += channel.dbus_cycle_congested;
    total_congested_count += channel.dbus_count_congested;
  }

  if (DRAM_CHANNELS > 1) {
    std::cout << " DBUS AVG_CONGESTED_CYCLE: ";
    if (total_congested_count)
      std::cout << std::setw(10) << ((double)total_congested_cycle / total_congested_count);
    else
      std::cout << "-";

    std::cout << std::endl;
  }
}

void reset_cache_stats(uint32_t cpu, CACHE* cache)
{
  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    cache->sim_access[cpu][i] = 0;
    cache->sim_hit[cpu][i] = 0;
    cache->sim_miss[cpu][i] = 0;
    cache->sim_partial_miss[cpu][i] = 0;
  }

  for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
    cache->holecount_hist[cpu][i] = 0;
    cache->holesize_hist[cpu][i] = 0;
    cache->cl_bytesaccessed_hist[cpu][i] = 0;
    cache->blsize_hist[cpu][i] = 0;
    cache->blsize_ignore_holes_hist[cpu][i] = 0;
  }

  cache->cl_accessmask_buffer.clear();
  cache->cl_num_accesses_to_complete_profile_buffer.clear();
  cache->pf_requested = 0;
  cache->pf_issued = 0;
  cache->pf_useful = 0;
  cache->pf_useless = 0;
  cache->pf_not_issued = 0;
  cache->pf_fill = 0;
  cache->USELESS_CACHELINE = 0;
  cache->TOTAL_CACHELINES = 0;

  cache->total_miss_latency = 0;

  cache->RQ_ACCESS = 0;
  cache->RQ_MERGED = 0;
  cache->RQ_TO_CACHE = 0;

  cache->WQ_ACCESS = 0;
  cache->WQ_MERGED = 0;
  cache->WQ_TO_CACHE = 0;
  cache->WQ_FORWARD = 0;
  cache->WQ_FULL = 0;

  if (cache->buffer) {
    BUFFER_CACHE* bc = &((VCL_CACHE*)cache)->buffer_cache;
    reset_cache_stats(cpu, bc);
  }
}

void finish_warmup()
{
  uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
  elapsed_minute -= elapsed_hour * 60;
  elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

  // reset core latency
  // note: since re-ordering he function calls in the main simulation loop, it's
  // no longer necessary to add
  //       extra latency for scheduling and execution, unless you want these
  //       steps to take longer than 1 cycle.
  // PAGE_TABLE_LATENCY = 100;
  // SWAP_LATENCY = 100000;

  cout << endl;
  for (uint32_t i = 0; i < NUM_CPUS; i++) {
    cout << "Warmup complete CPU " << i << " instructions: " << ooo_cpu[i]->num_retired << " cycles: " << ooo_cpu[i]->current_cycle;
    cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;
    cout << " Failed decodes: " << traces[i]->failed_decoding_instructions << " out of ";
    cout << traces[i]->total_decoding_instructions << " decoded instructions" << endl;
    cout << "Decode error rate: " << (100.0 * traces[i]->failed_decoding_instructions) / (double)traces[i]->total_decoding_instructions << "%" << endl;

    ooo_cpu[i]->begin_sim_cycle = ooo_cpu[i]->current_cycle;
    ooo_cpu[i]->begin_sim_instr = ooo_cpu[i]->num_retired;

    // reset branch stats
    ooo_cpu[i]->num_branch = 0;
    ooo_cpu[i]->branch_mispredictions = 0;
    ooo_cpu[i]->total_rob_occupancy_at_branch_mispredict = 0;

    for (uint32_t j = 0; j < 8; j++) {
      ooo_cpu[i]->total_branch_types[j] = 0;
      ooo_cpu[i]->branch_type_misses[j] = 0;
    }

    for (auto it = caches.rbegin(); it != caches.rend(); ++it)
      reset_cache_stats(i, *it);
  }
  cout << endl;

  // reset DRAM stats
  for (uint32_t i = 0; i < DRAM_CHANNELS; i++) {
    DRAM.channels[i].WQ_ROW_BUFFER_HIT = 0;
    DRAM.channels[i].WQ_ROW_BUFFER_MISS = 0;
    DRAM.channels[i].RQ_ROW_BUFFER_HIT = 0;
    DRAM.channels[i].RQ_ROW_BUFFER_MISS = 0;
  }
}

void signal_handler(int signal)
{
  cout << "Caught signal: " << signal << endl;
  exit(1);
}

int main(int argc, char** argv)
{
  // interrupt signal hanlder
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = signal_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  cout << endl << "*** ChampSim Multicore Out-of-Order Simulator ***" << endl << endl;

  // initialize knobs
  uint8_t show_heartbeat = 1;

  // check to see if knobs changed using getopt_long()
  int traces_encountered = 0;
  static struct option long_options[] = {{"warmup_instructions", required_argument, 0, 'w'},
                                         {"simulation_instructions", required_argument, 0, 'i'},
                                         {"hide_heartbeat", no_argument, 0, 'h'},
                                         {"cloudsuite", no_argument, 0, 'c'},
                                         {"ptrace", no_argument, 0, 'p'},
                                         {"intel", no_argument, 0, 'x'},
                                         {"ipoff", required_argument, 0, 'o'},
                                         {"nostallonmiss", no_argument, 0, 's'},
                                         {"result_dir", required_argument, 0, 'r'},
                                         {"no_detail_stats", no_argument, 0, 'd'},
                                         {"traces", no_argument, &traces_encountered, 1},
                                         {0, 0, 0, 0}};

  int c;
  while ((c = getopt_long_only(argc, argv, "w:i:hcp", long_options, NULL)) != -1 && !traces_encountered) {
    switch (c) {
    case 'w':
      warmup_instructions = atol(optarg);
      break;
    case 'i':
      simulation_instructions = atol(optarg);
      break;
    case 'h':
      show_heartbeat = 0;
      break;
    case 'd':
      knob_no_detail_stats = true;
      break;
    case 'c':
      knob_cloudsuite = 1;
      MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS_SPARC;
      break;
    case 'p':
      knob_pintrace = 1;
      break;
    case 'x':
      knob_intel = 1;
      break;
    case 'o':
      knob_ip_offset = atoi(optarg);
      break;
    case 's':
      knob_stall_on_miss = 0;
      break;
    case 'r':
      result_dir = optarg;
      break;
    case 0:
      break;
    default:
      abort();
    }
  }

  cout << "Warmup Instructions: " << warmup_instructions << endl;
  cout << "Simulation Instructions: " << simulation_instructions << endl;
  cout << "Number of CPUs: " << NUM_CPUS << endl;

  long long int dram_size = DRAM_CHANNELS * DRAM_RANKS * DRAM_BANKS * DRAM_ROWS * DRAM_COLUMNS * BLOCK_SIZE / 1024 / 1024; // in MiB
  std::cout << "Off-chip DRAM Size: ";
  if (dram_size > 1024)
    std::cout << dram_size / 1024 << " GiB";
  else
    std::cout << dram_size << " MiB";
  std::cout << " Channels: " << DRAM_CHANNELS << " Width: " << 8 * DRAM_CHANNEL_WIDTH << "-bit Data Rate: " << DRAM_IO_FREQ << " MT/s" << std::endl;

  std::cout << std::endl;
  std::cout << "VirtualMemory physical capacity: " << std::size(vmem.ppage_free_list) * vmem.page_size;
  std::cout << " num_ppages: " << std::size(vmem.ppage_free_list) << std::endl;
  std::cout << "VirtualMemory page size: " << PAGE_SIZE << " log2_page_size: " << LOG2_PAGE_SIZE << std::endl;

  std::cout << std::endl;
  for (int i = optind; i < argc; i++) {
    std::cout << "CPU " << traces.size() << " runs " << argv[i] << std::endl;

    traces.push_back(get_tracereader(argv[i], traces.size(), knob_cloudsuite, knob_pintrace));

    if (traces.size() > NUM_CPUS) {
      printf("\n*** Too many traces for the configured number of cores ***\n\n");
      assert(0);
    }
  }

  if (traces.size() != NUM_CPUS) {
    printf("\n*** Not enough traces for the configured number of cores ***\n\n");
    assert(0);
  }
  // end trace file setup

  // SHARED CACHE
  for (O3_CPU* cpu : ooo_cpu) {
    cpu->initialize_core();
  }

  for (auto it = caches.rbegin(); it != caches.rend(); ++it) {
    (*it)->impl_prefetcher_initialize();
    (*it)->impl_replacement_initialize();
  }

  // simulation entry point
  while (std::any_of(std::begin(simulation_complete), std::end(simulation_complete), std::logical_not<uint8_t>())) {

    uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
    elapsed_minute -= elapsed_hour * 60;
    elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

    for (auto op : operables) {
      try {
        op->_operate();
      } catch (champsim::deadlock& dl) {
        // ooo_cpu[dl.which]->print_deadlock();
        // std::cout << std::endl;
        // for (auto c : caches)
        for (auto c : operables) {
          c->print_deadlock();
          std::cout << std::endl;
        }

        abort();
      }
    }
    std::sort(std::begin(operables), std::end(operables), champsim::by_next_operate());

    for (std::size_t i = 0; i < ooo_cpu.size(); ++i) {
      // read from trace
      while (ooo_cpu[i]->fetch_stall == 0 && ooo_cpu[i]->instrs_to_read_this_cycle > 0 && !trace_ended[i]) {
        struct ooo_model_instr trace_inst;
        try {
          trace_inst = traces[i]->get();
          ooo_cpu[i]->num_read++;
        } catch (EndOfTraceException const& e) {
          if (ooo_cpu[i]->num_read >= (ooo_cpu[i]->begin_sim_instr + 50000000)) {
            trace_ended[i] = 1;
            break;
          }
          traces[i]->open();
        }
        ooo_cpu[i]->init_instruction(trace_inst);
      }

      // heartbeat information
      if (show_heartbeat && (ooo_cpu[i]->num_retired >= ooo_cpu[i]->next_print_instruction)) {
        float cumulative_ipc;
        if (warmup_complete[i])
          cumulative_ipc = (1.0 * (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle);
        else
          cumulative_ipc = (1.0 * ooo_cpu[i]->num_retired) / ooo_cpu[i]->current_cycle;
        float heartbeat_ipc = (1.0 * ooo_cpu[i]->num_retired - ooo_cpu[i]->last_sim_instr) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->last_sim_cycle);

        cout << "Heartbeat CPU " << i << " instructions: " << ooo_cpu[i]->num_retired << " cycles: " << ooo_cpu[i]->current_cycle;
        cout << " heartbeat IPC: " << heartbeat_ipc << " cumulative IPC: " << cumulative_ipc;
        cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;
        cout << " Failed decodes: " << traces[i]->failed_decoding_instructions << " out of ";
        cout << traces[i]->total_decoding_instructions << " decoded instructions" << endl;
        cout << "Decode error rate: " << (100.0 * traces[i]->failed_decoding_instructions) / (double)traces[i]->total_decoding_instructions << "%" << endl;

        ooo_cpu[i]->next_print_instruction += STAT_PRINTING_PERIOD;

        ooo_cpu[i]->last_sim_instr = ooo_cpu[i]->num_retired;
        ooo_cpu[i]->last_sim_cycle = ooo_cpu[i]->current_cycle;
      }

      // check for warmup
      // warmup complete
      if ((warmup_complete[i] == 0) && (ooo_cpu[i]->num_retired > warmup_instructions)) {
        warmup_complete[i] = 1;
        all_warmup_complete++;
      }
      if (all_warmup_complete == NUM_CPUS) { // this part is called only once
                                             // when all cores are warmed up
        all_warmup_complete++;
        finish_warmup();
      }
      // simulation complete
      if (((all_warmup_complete > NUM_CPUS) && (simulation_complete[i] == 0)
           && (ooo_cpu[i]->num_retired >= (ooo_cpu[i]->begin_sim_instr + simulation_instructions)))
          || trace_ended[i]) {
        simulation_complete[i] = 1;
        ooo_cpu[i]->finish_sim_instr = ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr;
        ooo_cpu[i]->finish_sim_cycle = ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle;

        cout << "Finished CPU " << i << " instructions: " << ooo_cpu[i]->finish_sim_instr << " cycles: " << ooo_cpu[i]->finish_sim_cycle;
        cout << " cumulative IPC: " << ((float)ooo_cpu[i]->finish_sim_instr / ooo_cpu[i]->finish_sim_cycle);
        cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;
        cout << " Failed decodes: " << traces[i]->failed_decoding_instructions << " out of ";
        cout << traces[i]->total_decoding_instructions << " decoded instructions" << endl;
        cout << "Decode error rate: " << (100.0 * traces[i]->failed_decoding_instructions) / (double)traces[i]->total_decoding_instructions << "%" << endl;
        cout << "Indirect branches: " << ooo_cpu[i]->indirect_branches << endl;

        for (auto it = caches.rbegin(); it != caches.rend(); ++it) {
          (*it)->record_remainder_cachelines(i);
          (*it)->write_buffers_to_disk();
          (*it)->print_private_stats();
          record_roi_stats(i, *it);
        }
      }
    }
  }

  uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
  elapsed_minute -= elapsed_hour * 60;
  elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

  cout << endl << "ChampSim completed all CPUs" << endl;
  if (NUM_CPUS > 1) {
    cout << endl << "Total Simulation Statistics (not including warmup)" << endl;
    for (uint32_t i = 0; i < NUM_CPUS; i++) {
      cout << endl
           << "CPU " << i
           << " cumulative IPC: " << (float)(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle);
      cout << " instructions: " << ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr
           << " cycles: " << ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle << endl;
      for (auto it = caches.rbegin(); it != caches.rend(); ++it)
        print_sim_stats(i, *it);
    }
  }

  cout << endl << "Region of Interest Statistics" << endl;
  for (uint32_t i = 0; i < NUM_CPUS; i++) {
    cout << endl << "CPU " << i << " cumulative IPC: " << ((float)ooo_cpu[i]->finish_sim_instr / ooo_cpu[i]->finish_sim_cycle);
    cout << " instructions: " << ooo_cpu[i]->finish_sim_instr << " cycles: " << ooo_cpu[i]->finish_sim_cycle << endl;
    cout << "CPU " << i << " FRONTEND STALLED CYCLES:\t" << ooo_cpu[i]->frontend_stall_cycles << endl;
    for (auto it = caches.rbegin(); it != caches.rend(); ++it)
      print_roi_stats(i, *it);
  }

  for (auto it = caches.rbegin(); it != caches.rend(); ++it)
    (*it)->impl_prefetcher_final_stats();

  for (auto it = caches.rbegin(); it != caches.rend(); ++it)
    (*it)->impl_replacement_final_stats();

#ifndef CRC2_COMPILE
  print_dram_stats();
  print_branch_stats();
#endif

  return 0;
}
