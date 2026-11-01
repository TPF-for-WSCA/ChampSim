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

// Demand-side branch callbacks must not inject trace-driven targets into FDIP.
void fdip::prefetcher_branch_operate(champsim::address, uint8_t, champsim::address) {}

void fdip::prefetcher_redirect()
{
  queued_targets.clear();
  // Already-issued requests remain in the memory hierarchy.
}

void fdip::prefetcher_prediction_operate(champsim::address branch_target)
{
  if (branch_target == champsim::address{})
    return;

  const champsim::block_number target_block{branch_target};
  const bool already_queued = std::any_of(queued_targets.begin(), queued_targets.end(), [&](auto target) {
    // Different targets in one UBS line can have different coverage.
    return intern_->ubs ? target == branch_target : champsim::block_number{target} == target_block;
  });

  if (!already_queued && !was_recently_prefetched(target_block)) {
    queued_targets.push_back(branch_target);
    if (queued_targets.size() > MAX_QUEUED_TARGETS)
      queued_targets.pop_front();
  }
}

uint32_t fdip::prefetcher_cache_operate(champsim::address addr, champsim::address, uint8_t cache_hit, bool, access_type type, uint32_t metadata_in)
{
  // A completed UBS prefetch must not hide a later miss to discarded bytes.
  if (intern_->ubs && type == access_type::PREFETCH && cache_hit)
    forget_prefetch(champsim::block_number{addr});
  if (cache_hit == 0 && intern_->get_mshr_occupancy_ratio() < 0.5) {
    const champsim::block_number next_block = champsim::block_number{addr} + 1;
    if (!was_recently_prefetched(next_block) && prefetch_line(champsim::address{next_block}, true, metadata_in))
      remember_prefetch(next_block);
  }

  return metadata_in;
}

void fdip::forget_prefetch(champsim::block_number block)
{
  recent_prefetches.erase(std::remove(recent_prefetches.begin(), recent_prefetches.end(), block), recent_prefetches.end());
}

uint32_t fdip::prefetcher_cache_fill(champsim::address addr, long, long, uint8_t, champsim::address, uint32_t metadata_in)
{
  if (intern_->ubs)
    forget_prefetch(champsim::block_number{addr});
  return metadata_in;
}

void fdip::prefetcher_cycle_operate()
{
  while (!queued_targets.empty() && intern_->get_mshr_occupancy_ratio() < 0.5) {
    const auto target = queued_targets.front();
    const champsim::block_number target_block{target};
    if (intern_->is_instruction_present(target) || was_recently_prefetched(target_block)) {
      queued_targets.pop_front();
      continue;
    }
    if (prefetch_line(intern_->ubs ? target : champsim::address{target_block}, true, 0)) {
      remember_prefetch(target_block);
      queued_targets.pop_front();
    }
    return; // At most one new prefetch per cycle.
  }
}
