//=======================================================================================//
// File             : sms/sms.cc
// Author           : Rahul Bera, SAFARI Research Group (write2bera@gmail.com)
// Date             : 19/AUG/2025
// Description      : Implements Spatial Memory Streaming prefetcher, ISCA'06
//=======================================================================================//

#include "cache.h"
#include "sms.h"

sms current;

void CACHE::prefetcher_initialize()
{
  /* init PHT */
  std::deque<PHTEntry*> d;
  current.pht.resize(current.PHT_SETS, d);
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  uint64_t page = addr >> current.REGION_SIZE_LOG;
  uint32_t offset = (uint32_t)((addr >> LOG2_BLOCK_SIZE) & ((1ull << (current.REGION_SIZE_LOG - LOG2_BLOCK_SIZE)) - 1));
  std::vector<uint64_t> pref_addr;

  // cout << "pc " << hex << setw(16) << pc
  // 	<< " address " << hex << setw(16) << address
  // 	<< " page " << hex << setw(16) << page
  // 	<< " offset " << dec << setw(2) << offset
  // 	<< endl;

  auto at_index = current.search_acc_table(page);
  //   stats.at.lookup++;
  if (at_index != current.acc_table.end()) {
    /* accumulation table hit */
    // stats.at.hit++;
    (*at_index)->pattern[offset] = 1;
    current.update_age_acc_table(at_index);
  } else {
    /* search filter table */
    auto ft_index = current.search_filter_table(page);
    // stats.ft.lookup++;
    if (ft_index != current.filter_table.end()) {
      /* filter table hit */
      //   stats.ft.hit++;
      current.insert_acc_table((*ft_index), offset);
      current.evict_filter_table(ft_index);
    } else {
      /* filter table miss. Beginning of new generation. Issue prefetch */
      current.insert_filter_table(ip, page, offset);
      current.generate_prefetch(ip, addr, page, offset, pref_addr);
      current.buffer_prefetch(pref_addr);
    }
  }
  return 0;
}


uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {current.issue_prefetch(this);}

void CACHE::prefetcher_final_stats() {}