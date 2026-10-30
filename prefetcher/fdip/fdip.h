#ifndef PREFETCHER_FDIP_H
#define PREFETCHER_FDIP_H

#include <cstddef>
#include <cstdint>
#include <deque>

#include "access_type.h"
#include "address.h"
#include "modules.h"

struct fdip : public champsim::modules::prefetcher {
  static constexpr std::size_t MAX_QUEUED_TARGETS = 128;
  static constexpr std::size_t MAX_RECENT_PREFETCHES = 4;

  std::deque<champsim::address> queued_targets;
  std::deque<champsim::block_number> recent_prefetches;

  using prefetcher::prefetcher;

  void prefetcher_branch_operate(champsim::address ip, uint8_t branch_type, champsim::address branch_target);
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                    uint32_t metadata_in);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
  void prefetcher_cycle_operate();

private:
  [[nodiscard]] bool was_recently_prefetched(champsim::block_number block) const;
  void remember_prefetch(champsim::block_number block);
};

#endif
