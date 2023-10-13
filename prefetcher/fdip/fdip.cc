#include <algorithm>
#include <math.h>
#include <stack>

#include "cache.h"
#include "ooo_cpu.h"
#define MAX_PFETCHQ_ENTRIES 48
#define MAX_RECENT_PFETCH 10

std::deque<uint64_t> prefetch_queue;    // Storage: 64-bits * 48 (queue size) = 384 bytes
std::deque<uint64_t> recent_prefetches; // Storage: 64-bits * 10 (queue size) = 80 bytes

void O3_CPU::prefetcher_initialize() {}

void O3_CPU::prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target)
{
  uint64_t block_addr = ((ip >> LOG2_BLOCK_SIZE) << LOG2_BLOCK_SIZE);
  if (block_addr == 0)
    return;

  std::deque<uint64_t>::iterator it = std::find(prefetch_queue.begin(), prefetch_queue.end(), block_addr);
  if (it == prefetch_queue.end()) {
    std::deque<uint64_t>::iterator it1 = std::find(recent_prefetches.begin(), recent_prefetches.end(), block_addr);
    if (it1 == recent_prefetches.end()) {
      prefetch_queue.push_back(block_addr);
    }
  }
}

uint32_t O3_CPU::prefetcher_cache_operate(uint64_t v_addr, uint8_t cache_hit, uint8_t prefetch_hit, uint32_t metadata_in)
{
#define L1I (static_cast<CACHE*>(L1I_bus.lower_level))
  if ((cache_hit == 0) && (L1I->get_occupancy(0, 0) < L1I->MSHR_SIZE >> 1)) {
    uint64_t pf_addr = v_addr + (1 << LOG2_BLOCK_SIZE);
    prefetch_code_line(pf_addr);
  }
  return metadata_in;
}

void O3_CPU::prefetcher_cycle_operate()
{
  if (prefetch_queue.size()) {
#define L1I (static_cast<CACHE*>(L1I_bus.lower_level))
    if (L1I->get_occupancy(0, 0) < L1I->MSHR_SIZE >> 1) {
      prefetch_code_line(prefetch_queue.front());
      recent_prefetches.push_back(prefetch_queue.front());
      if (recent_prefetches.size() > MAX_RECENT_PFETCH) {
        recent_prefetches.pop_front();
      }
      prefetch_queue.pop_front();
    }
  }
}

uint32_t O3_CPU::prefetcher_cache_fill(uint64_t v_addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_v_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void O3_CPU::prefetcher_final_stats() {}