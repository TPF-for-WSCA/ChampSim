#include "srrip.h"

#include <algorithm>
#include <cassert>
#include <unordered_map>

#include "cache.h"

srrip::srrip(CACHE* cache) : srrip(cache, cache->NUM_SET, cache->NUM_WAY) {}

srrip::srrip(CACHE* cache, long sets_, long ways_) : replacement(cache)
{
  std::generate_n(std::back_inserter(sets), sets_, [ways = ways_] { return srrip_set_helper{ways}; });
}

// find replacement victim
long srrip::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                        champsim::address full_addr, access_type type)
{
  return sets.at(static_cast<std::size_t>(set)).victim();
}

// Miss lookups can have way == NUM_WAY; insertion is handled by the fill hook.
void srrip::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                     champsim::address victim_addr, access_type type, uint8_t hit)
{
  if (hit)
    sets.at(static_cast<std::size_t>(set)).update(way, true);
}

std::optional<long> srrip::find_victim_in_range(long set, long first, long last)
{
  return sets.at(static_cast<std::size_t>(set)).victim(first, last);
}

void srrip::replacement_cache_fill(uint32_t, long set, long way, champsim::address, champsim::address, champsim::address, access_type)
{
  // Use an explicit fill hook: miss lookups have no selected victim yet, and
  // the legacy update-only adapter does not deliver insertion updates here.
  sets.at(static_cast<std::size_t>(set)).update(way, false);
}

srrip_set_helper::srrip_set_helper(long ways) : rrpv_values(static_cast<std::size_t>(ways), maxRRPV) {}

auto srrip_set_helper::get_rrpv(long way) -> rrpv_type& { return rrpv_values.at(static_cast<std::size_t>(way)); }

long srrip_set_helper::victim() { return victim(0, static_cast<long>(rrpv_values.size())); }

long srrip_set_helper::victim(long first, long last)
{
  assert(first >= 0 && first < last && static_cast<std::size_t>(last) <= rrpv_values.size());
  auto begin = std::next(std::begin(rrpv_values), first);
  auto end = std::next(std::begin(rrpv_values), last);
  auto victim = std::max_element(begin, end);
  // Age only eligible ways. Unrelated size classes retain their RRPVs.
  const auto diff = maxRRPV - *victim;
  std::transform(begin, end, begin, [diff](auto x) { return x + diff; });
  return std::distance(std::begin(rrpv_values), victim);
}

void srrip_set_helper::update(long way, bool hit) { get_rrpv(way) = hit ? 0 : (maxRRPV - 1); }
