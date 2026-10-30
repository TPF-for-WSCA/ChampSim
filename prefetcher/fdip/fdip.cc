#include "fdip.h"

#include <algorithm>

#include "cache.h"

bool fdip::was_recently_prefetched(champsim::block_number block) const
{
  return std::find(recent_prefetches.begin(), recent_prefetches.end(), block) != recent_prefetches.end();
}

void fdip::remember_prefetch(champsim::block_number block)
{
  recent_prefetches.push_back(block);
  if (recent_prefetches.size() > MAX_RECENT_PREFETCHES)
    recent_prefetches.pop_front();
}

void fdip::prefetcher_branch_operate(champsim::address, uint8_t, champsim::address branch_target)
{
  if (branch_target == champsim::address{})
    return;

  const champsim::block_number target_block{branch_target};
  const bool already_queued =
      std::find_if(queued_targets.begin(), queued_targets.end(), [target_block](auto target) { return champsim::block_number{target} == target_block; })
      != queued_targets.end();

  if (!already_queued && !was_recently_prefetched(target_block)) {
    queued_targets.push_back(branch_target);
    if (queued_targets.size() > MAX_QUEUED_TARGETS)
      queued_targets.pop_front();
  }
}

uint32_t fdip::prefetcher_cache_operate(champsim::address addr, champsim::address, uint8_t cache_hit, bool, access_type, uint32_t metadata_in)
{
  if (cache_hit == 0 && intern_->get_mshr_occupancy_ratio() < 0.5) {
    const champsim::block_number next_block = champsim::block_number{addr} + 1;
    if (!was_recently_prefetched(next_block) && prefetch_line(champsim::address{next_block}, true, metadata_in))
      remember_prefetch(next_block);
  }

  return metadata_in;
}

uint32_t fdip::prefetcher_cache_fill(champsim::address, long, long, uint8_t, champsim::address, uint32_t metadata_in) { return metadata_in; }

void fdip::prefetcher_cycle_operate()
{
  while (!queued_targets.empty() && intern_->get_mshr_occupancy_ratio() < 0.5) {
    const auto target = queued_targets.front();
    const champsim::block_number target_block{target};
    if (intern_->is_instruction_present(target) || was_recently_prefetched(target_block)) {
      queued_targets.pop_front();
      continue;
    }
    if (prefetch_line(champsim::address{target_block}, true, 0)) {
      remember_prefetch(target_block);
      queued_targets.pop_front();
    }
    return; // At most one new prefetch per cycle.
  }
}
