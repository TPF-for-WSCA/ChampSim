#include <algorithm>
#include <iterator>
#include <set>

#include "cache.h"
#include "util.h"

void CACHE::initialize_replacement() {}

uint8_t CACHE::get_insert_pos(LruModifier lru_modifier, uint32_t set)
{
  std::set<uint32_t> avail_lru_positions;
  auto begin = std::next(block.begin(), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);
  for (; begin < end; begin++) {
    avail_lru_positions.insert(begin->lru);
  }
  auto end = avail_lru_positions.rbegin();
  if (lru_modifier >= 100000) {
    if (avail_lru_positions.size() < active_inserts)
      return 0;
    return (end + active_inserts)->lru;
  }
  if (lru_modifier >= 10000) {
    if (avail_lru_positions.size() < 4)
      return 0;
    return (end + 4)->lru;
  } else if (lru_modifier >= 1000) {
    if (avail_lru_positions.size() < 3)
      return 0;
    return (end + 3)->lru;
  } else if (lru_modifier >= 100) {
    if (avail_lru_positions.size() < 2)
      return 0;
    return (end + 2)->lru;
  } else if (lru_modifier >= 10) {
    if (avail_lru_positions.size() < 1)
      return 0;
    return (end + 1)->lru;
  }
  return 0;
}

// find replacement victim
uint32_t CACHE::find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
  // baseline LRU
  return std::distance(current_set, std::max_element(current_set, std::next(current_set, NUM_WAY), lru_comparator<BLOCK, BLOCK>()));
}

// called on every cache hit and cache fill
void CACHE::update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type,
                                     uint8_t hit)
{
  if (hit && type == WRITEBACK)
    return;

  uint32_t lru_pos = get_insert_pos(lru_modifier, set);
  if (hit)
    lru_pos = 0;
  if (type == FILL && lru_modifier >= 10 && not is_default_lru(lru_modifier)) {
    auto begin = std::next(block.begin(), set * NUM_WAY);
    auto end = std::next(begin, lru_subset.second);
    begin = std::next(begin, lru_subset.first);
    uint32_t waycount = NUM_WAY;
    std::for_each(begin, end, [lru_pos, waycount](BLOCK& x) {
      if (lru_pos >= x.lru && x.lru - 1 < waycount) {
        x.lru++;
      }
    });
    std::next(begin, way)->lru = lru_pos;
  } else {
    auto begin = std::next(block.begin(), set * NUM_WAY);
    auto end = std::next(begin, NUM_WAY);
    uint32_t hit_lru = std::next(begin, way)->lru;
    auto num_ways = NUM_WAY;
    std::for_each(begin, end, [hit_lru, num_ways, lru_pos](BLOCK& x) {
      if (x.lru >= num_ways)
        return;
      if (x.lru <= hit_lru and not lru_pos)
        x.lru++;
      else if (lru_pos and x.lru >= lru_pos)
        x.lru++; // shift upwards if we insert at not mru
    });

    std::next(begin, way)->lru = lru_pos;
  }
}

void CACHE::replacement_final_stats() {}
