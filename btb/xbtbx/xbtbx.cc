
/*
 * This file implements a basic Branch Target Buffer (BTB) structure.
 * It uses a set-associative BTB to predict the targets of non-return branches,
 * and it uses a small Return Address Stack (RAS) to predict the target of
 * returns.
 */

#include <algorithm>
#include <bitset>
#include <cmath>
#include <deque>
#include <iostream>
#include <map>
#include <numeric>
#include <set>

#include "msl/lru_table.h"
#include "ooo_cpu.h"

#define SMALL_BIG_WAY_SPLIT 14

constexpr uint64_t pow2(uint8_t exp)
{
  assert(exp <= 64);
  uint64_t result = 1;
  while (exp) {
    result *= 2;
    exp -= 1;
  }
  return result;
}
namespace
{

enum class branch_info {
  INDIRECT,
  RETURN,
  ALWAYS_TAKEN,
  CONDITIONAL,
};

// DEBUGGING VARIABLES
// size_t _37_added = 0;
// size_t _37_removed = 0;
// size_t _37_deleted = 0;
// size_t _37_updated = 0;
// DEBUGGING END

std::map<uint64_t, uint64_t> region_tag_entry_count = {};
std::vector<uint8_t> index_bits;
std::vector<uint8_t> tag_bits;
std::vector<uint8_t> btb_addressing_hash;
std::vector<uint64_t> btb_addressing_masks;
std::vector<int> btb_addressing_shifts; // negative shift amounts are left shifts by the given amount. 0 means no shift is necessary, positive shift amounts
                                        // are shifts to the right (max the bit idx)
std::size_t _INDEX_MASK = 0;
std::size_t _TAG_MASK = 0;
std::size_t _FULL_TAG_MASK = 0;
std::size_t _REGION_MASK = 0;
std::size_t _BTB_SETS = 0;
std::size_t _BTB_WAYS = 0;
uint8_t _BTB_CLIPPED_TAG = 0;
uint8_t _BTB_TAG_SIZE = 0;
uint8_t _BTB_SET_BITS;
uint16_t _BTB_TAG_REGIONS = 0;
uint8_t _BTB_TAG_REGION_SIZE = 0;
uint16_t _BTB_REGION_BITS = 0;
constexpr std::size_t BTB_INDIRECT_SIZE = 4096;
constexpr std::size_t RAS_SIZE = 64;
constexpr std::size_t CALL_SIZE_TRACKERS = 1024;
bool small_way_regions_enabled = 0;
bool big_way_regions_enabled = 0;

uint64_t prev_branch_ip = 0;
std::map<uint32_t, uint64_t> offset_reuse_freq;
std::map<uint64_t, std::set<uint8_t>> offset_sizes_by_target;
std::set<uint64_t> branch_ip;
std::array<uint64_t, 64> dynamic_bit_counts;
std::array<uint64_t, 64> static_bit_counts;

// TODO: Only makes sense with BTB-X
bool utilise_regions(size_t way_size)
{
  if (small_way_regions_enabled && big_way_regions_enabled) {
    return true;
  }
  if (small_way_regions_enabled) {
    return way_size <= SMALL_BIG_WAY_SPLIT;
  } else if (big_way_regions_enabled) {
    return SMALL_BIG_WAY_SPLIT < way_size;
  } else {
    return false;
  }
}

enum BTB_ReplacementStrategy { LRU, REF0, REF };

struct BTBEntry {
  uint64_t ip_tag = 0;
  uint64_t target;
  branch_info type = branch_info::ALWAYS_TAKEN;
  uint16_t offset_tag = 0;
  uint8_t target_size = 64; // TODO: Only update for which we have sizes
  uint64_t offset_mask = -1;

  // TODO: shift indexes and tags into place
  auto index() const
  {
    if (btb_addressing_hash.empty())
      return (ip_tag >> 2) & _INDEX_MASK;
    assert(btb_addressing_hash.size() <= _BTB_SETS);
    auto ip = ip_tag >> 2;
    uint64_t idx = 0;
    for (uint8_t i = 0; i < _BTB_SET_BITS; i++) {
      auto mask = btb_addressing_masks[i];
      auto shift = btb_addressing_shifts[i];
      auto bit = ip & mask;
      if (shift > 0) {
        bit >>= shift;
      } else if (shift < 0) {
        bit <<= (-1 * shift);
      }
      idx |= bit;
    }
    return idx;
  }
  auto tag() const
  {
    auto tag = ip_tag >> 2 >> _BTB_SET_BITS;
    if (!_BTB_CLIPPED_TAG) {
      return tag;
    }
    if (btb_addressing_hash.empty()) {
      tag &= _FULL_TAG_MASK;
    } else {
      tag = 0;
      auto ip = ip_tag >> 2;
      for (uint8_t i = _BTB_SET_BITS; i < _BTB_SET_BITS + _BTB_TAG_SIZE; i++) {
        auto mask = btb_addressing_masks[i];
        auto shift = btb_addressing_shifts[i];
        auto bit = ip & mask;
        if (shift > 0) {
          bit >>= shift;
        } else if (shift < 0) {
          bit <<= (-1 * shift);
        }
        tag |= bit;
      }
      tag = tag >> _BTB_SET_BITS;
    }
    if (ip_tag && _BTB_TAG_REGIONS && utilise_regions(target_size)) {
      // TODO: double check if the shift amount of the BTB TAG size is correct and we are not overriding the actual tag bits
      auto masked_bits = tag & (_REGION_MASK << _BTB_TAG_SIZE);
      tag ^= masked_bits;
      tag |= offset_tag << _BTB_TAG_SIZE;
    }
    return tag;
  }

  auto partial_tag() const
  {
    uint64_t tag = ip_tag >> 2 >> _BTB_SET_BITS;
    if (!_BTB_CLIPPED_TAG) {
      return tag;
    }
    if (btb_addressing_hash.empty())
      tag &= _TAG_MASK;
    else {
      tag = 0;
      auto ip = ip_tag >> 2;
      for (uint8_t i = _BTB_SET_BITS; i < _BTB_SET_BITS + _BTB_TAG_SIZE; i++) {
        auto mask = btb_addressing_masks[i];
        auto shift = btb_addressing_shifts[i];
        auto bit = ip & mask;
        if (shift > 0) {
          bit >>= shift;
        } else if (shift < 0) {
          bit <<= (-1 * shift);
        }
        tag |= bit;
      }
      tag = tag >> _BTB_SET_BITS;
    }
    return tag;
  }

  auto get_prediction() const
  {
    auto offset = target & offset_mask;
    auto prediction = ip_tag & (~offset_mask);
    prediction |= offset;
    return prediction;
  }
};

struct region_btb_entry_t {
  uint64_t ip_tag = 0;
  auto index() const { return 0; }
  auto tag() const
  {
    // TODO: calculate region tag
    auto tag = ip_tag >> 2 >> _BTB_SET_BITS >> _BTB_TAG_SIZE;
    tag &= _REGION_MASK;
    return tag;
  }
  auto partial_tag() const { return 0; }
};

// TODO: Currently not implemented -- Add later on
// extern bool is_kernel(uint64_t ip);
// extern bool is_shared_or_vdso(uint64_t ip);

std::map<O3_CPU*, champsim::msl::lru_table<BTBEntry>> BTB;
std::map<O3_CPU*, champsim::msl::lru_table<region_btb_entry_t>> REGION_BTB;
std::map<O3_CPU*, std::array<uint64_t, BTB_INDIRECT_SIZE>> INDIRECT_BTB;
std::map<O3_CPU*, std::bitset<champsim::lg2(BTB_INDIRECT_SIZE)>> CONDITIONAL_HISTORY;
std::map<O3_CPU*, std::deque<uint64_t>> RAS;
std::map<O3_CPU*, std::array<uint64_t, CALL_SIZE_TRACKERS>> CALL_SIZE;
} // namespace

void O3_CPU::initialize_btb()
{
  ::BTB.insert({this, champsim::msl::lru_table<BTBEntry>{BTB_SETS, BTB_WAYS}});
  ::REGION_BTB.insert({this, champsim::msl::lru_table<region_btb_entry_t>{1, BTB_TAG_REGIONS}});
  std::fill(std::begin(::INDIRECT_BTB[this]), std::end(::INDIRECT_BTB[this]), 0);
  ::btb_addressing_hash = btb_index_tag_hash;
  btb_addressing_masks.resize(btb_addressing_hash.size());
  btb_addressing_shifts.resize(btb_addressing_hash.size());
  for (size_t i = 0; i < btb_index_tag_hash.size(); i++) {
    btb_addressing_masks[i] = ((uint64_t)1) << btb_index_tag_hash[i];
    btb_addressing_shifts[i] = (btb_index_tag_hash[i] > i) ? (btb_index_tag_hash[i] - i) : -1 * (i - (int)btb_index_tag_hash[i]);
  }
  std::fill(std::begin(::CALL_SIZE[this]), std::end(::CALL_SIZE[this]), 4);
  ::CONDITIONAL_HISTORY[this] = 0;
  _BTB_SET_BITS = champsim::lg2(BTB_SETS);
  if (this->BTB_CLIPPED_TAG) {
    _BTB_CLIPPED_TAG = 1;
    _BTB_TAG_SIZE = this->BTB_TAG_SIZE;
    _BTB_TAG_REGIONS = this->BTB_TAG_REGIONS;
    _BTB_TAG_REGION_SIZE = this->BTB_TAG_REGION_SIZE;
    _REGION_MASK = _BTB_TAG_REGIONS ? (pow2(_BTB_TAG_REGION_SIZE) - 1) : (pow2(62 - _BTB_SET_BITS - _BTB_TAG_SIZE) - 1);
    _BTB_REGION_BITS = champsim::lg2(_BTB_TAG_REGIONS);
  } else {
    _BTB_TAG_SIZE = 62 - _BTB_SET_BITS;
    _REGION_MASK = pow2(62 - _BTB_SET_BITS) - 1;
  }
  // TODO: Initialize index and tag based on bit information
  _INDEX_MASK = BTB_SETS - 1;
  _BTB_SETS = BTB_SETS;
  _BTB_WAYS = BTB_WAYS;
  small_way_regions_enabled = btb_small_way_regions_enabled;
  big_way_regions_enabled = btb_big_way_regions_enabled;

  _TAG_MASK = pow2(_BTB_TAG_SIZE) - 1;
  _FULL_TAG_MASK = pow2(_BTB_TAG_SIZE + _BTB_TAG_REGION_SIZE) - 1;
  if (BTB_TARGET_SIZES.size() == BTB_WAYS) {
    for (uint16_t i = 0; i < BTB_SETS; i++) {
      auto [set_begin, set_end] = ::BTB.at(this).get_set_span(i);
      auto way_sizes_begin = std::begin(BTB_TARGET_SIZES);
      auto way_sizes_end = std::end(BTB_TARGET_SIZES);
      for (; way_sizes_begin != way_sizes_end && set_begin != set_end; set_begin++) {
        set_begin->data.target_size = *way_sizes_begin;
        set_begin->data.offset_mask = pow2(*way_sizes_begin) - 1;
        way_sizes_begin++;
      }
    }
  }
}

std::tuple<uint64_t, uint64_t, uint8_t> O3_CPU::btb_prediction(uint64_t ip)
{
  std::optional<::BTBEntry> btb_entry = std::nullopt;
  if (_BTB_TAG_REGIONS) {
    auto region_idx_ = ::REGION_BTB.at(this).check_hit_idx({ip});
    auto partial_btb_entry = ::BTB.at(this).check_hit({ip, 0, ::branch_info::ALWAYS_TAKEN, 0}, true);
    if (BTB_PARTIAL_TAG_RESOLUTION || !utilise_regions(partial_btb_entry.value_or(::BTBEntry{ip, 0, ::branch_info::CONDITIONAL, 0, 0}).target_size)) {
      btb_entry = partial_btb_entry;
    }
    if (region_idx_.has_value()) {
      auto entry = ::BTB.at(this).check_hit({ip, 0, ::branch_info::ALWAYS_TAKEN, region_idx_.value()});
      if (entry.has_value()) {
        btb_entry = entry;
      }
    }
  } else {
    btb_entry = ::BTB.at(this).check_hit({ip, 0, ::branch_info::ALWAYS_TAKEN, 0});
  }

  // no prediction for this IP
  // default: no aliasing, thus returning ip itself as recorded ip
  if (!btb_entry.has_value())
    return {0, ip, false};

  if (btb_entry->type == ::branch_info::RETURN) {
    if (std::empty(::RAS[this]))
      return {0, btb_entry->ip_tag, true};

    // peek at the top of the RAS and adjust for the size of the call instr
    auto target = ::RAS[this].back();
    auto size = ::CALL_SIZE[this][target % std::size(::CALL_SIZE[this])];

    return {target + size, btb_entry->ip_tag, true};
  }
  /*
    if (btb_entry->type == ::branch_info::INDIRECT) {
      auto hash = (ip >> 2) ^ ::CONDITIONAL_HISTORY[this].to_ullong();
      return {::INDIRECT_BTB[this][hash % std::size(::INDIRECT_BTB[this])], btb_entry->ip_tag, true};
    }*/

  return {btb_entry->get_prediction(), btb_entry->ip_tag, btb_entry->type != ::branch_info::CONDITIONAL || btb_entry->type != ::branch_info::INDIRECT};
}

// TODO: ONLY UPDATE WHEN FITTING IN THE WAY
void O3_CPU::update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  // DONE: calculate size
  uint64_t offset_size = ip ^ branch_target;
  uint8_t num_bits = 0;
  while (offset_size) {
    offset_size >>= 1;
    num_bits++;
  }

  // TODO: Integrate with sim
  bool is_static = branch_ip.insert(ip).second;
  for (int j = 0; j < 64; j++) {
    if ((ip >> j) & 0x1) {
      static_bit_counts[j] += (is_static) ? 1 : 0;
      dynamic_bit_counts[j] += 1;
    }
  }

  // add something to the RAS
  if (branch_type == BRANCH_DIRECT_CALL || branch_type == BRANCH_INDIRECT_CALL) {
    RAS[this].push_back(ip);
    if (std::size(RAS[this]) > RAS_SIZE)
      RAS[this].pop_front();
  }

  // updates for indirect branches
  // if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL)) {
  //   auto hash = (ip >> 2) ^ ::CONDITIONAL_HISTORY[this].to_ullong();
  //   ::INDIRECT_BTB[this][hash % std::size(::INDIRECT_BTB[this])] = branch_target;
  // }

  if ((branch_type == BRANCH_CONDITIONAL) || (branch_type == BRANCH_OTHER)) {
    ::CONDITIONAL_HISTORY[this] <<= 1;
    ::CONDITIONAL_HISTORY[this].set(0, taken);
  }

  if (branch_type == BRANCH_RETURN && !std::empty(::RAS[this])) {
    // recalibrate call-return offset if our return prediction got us close, but not exact
    auto call_ip = ::RAS[this].back();
    ::RAS[this].pop_back();

    auto estimated_call_instr_size = (call_ip > branch_target) ? call_ip - branch_target : branch_target - call_ip;
    if (estimated_call_instr_size <= 10) {
      ::CALL_SIZE[this][call_ip % std::size(::CALL_SIZE[this])] = estimated_call_instr_size;
    }
    num_bits = 0; // We don't need to store the target, we store it in the RAS
  }

  // update btb entry
  auto type = ::branch_info::ALWAYS_TAKEN;
  if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL))
    type = ::branch_info::INDIRECT;
  else if (branch_type == BRANCH_RETURN)
    type = ::branch_info::RETURN;
  else if ((branch_type == BRANCH_CONDITIONAL) || (branch_type == BRANCH_OTHER))
    type = ::branch_info::CONDITIONAL;

  // TODO: Only update region btb if partial was wrong?
  std::optional<uint8_t> region_idx = std::nullopt;
  std::optional<::BTBEntry> opt_entry;
  uint8_t entry_size = num_bits;
  // TODO: ADD REGION INFORMATION IF AVAILABLE
  std::optional<uint8_t> tmp_region_idx = ::REGION_BTB.at(this).check_hit_idx({ip});
  std::optional<::BTBEntry> small_hit = ::BTB.at(this).check_hit({ip, 0, type, tmp_region_idx.value_or(-1), 0});
  std::optional<::BTBEntry> big_hit = ::BTB.at(this).check_hit({ip, 0, type, tmp_region_idx.value_or(-1), 64});
  ::BTBEntry lru_elem = ::BTB.at(this).get_lru_elem(::BTBEntry{ip, 0}, num_bits);
  // TODO: pass way size and not num bits to utilise regions function here
  // TODO: check partial and check if utilise region is true -> update that value
  bool small_region = small_hit.has_value() && num_bits <= small_hit.value().target_size && utilise_regions(small_hit.value().target_size);
  bool big_region = big_hit.has_value() && num_bits <= big_hit.value().target_size && utilise_regions(big_hit.value().target_size);
  bool lru_region = utilise_regions(lru_elem.target_size);
  bool require_region = small_region || big_region || lru_region;
  if (require_region) {
    region_idx = ::REGION_BTB.at(this).check_hit_idx({ip});
    if (!region_idx.has_value() && BTB_PARTIAL_TAG_RESOLUTION) {
      opt_entry = ::BTB.at(this).check_hit({ip, branch_target, type, 0}, true);
      if (!opt_entry.has_value() || (opt_entry.has_value() && opt_entry.value().get_prediction() != branch_target)) {
        ::REGION_BTB.at(this).fill(::region_btb_entry_t{ip});
        region_idx = ::REGION_BTB.at(this).check_hit_idx({ip});
      }
    } else if (!region_idx.has_value()) {
      ::REGION_BTB.at(this).fill(::region_btb_entry_t{ip});
      region_idx = ::REGION_BTB.at(this).check_hit_idx({ip});
    }
    if (small_region)
      entry_size = std::max(entry_size, small_hit.value().target_size);
    if (big_region)
      entry_size = std::max(entry_size, big_hit.value().target_size);
    if (!small_region && !big_region)
      entry_size = std::max(entry_size, lru_elem.target_size);
  } else {
    if (small_hit.has_value()) {
      entry_size = std::max(entry_size, small_hit.value().target_size);
    }
    if (big_hit.has_value()) {
      entry_size = std::max(entry_size, big_hit.value().target_size);
    }
    if (!small_hit.has_value() && !big_hit.has_value()) {
      entry_size = lru_elem.target_size;
    }
  }

  /*   opt_entry = std::nullopt;
    if (region_idx.has_value())
      opt_entry = ::BTB.at(this).check_hit({ip, branch_target, type, region_idx.value()});
    if (opt_entry.has_value()) {
      opt_entry->type = type;
      if (branch_target != 0)
        opt_entry->target = branch_target;
    }
   */

  /********* STATS ACCOUNTING *********/
  std::optional<::BTBEntry> replaced_entry = std::nullopt;
  if (branch_target != 0) {
    // TODO: Check if (since we already know about region or not region) should make two distinct calls out of the below
    replaced_entry = ::BTB.at(this).fill(
        ::BTBEntry{ip, branch_target, type, region_idx.value_or(pow2(_BTB_REGION_BITS)), entry_size},
        num_bits); // ASSIGN to region 2^BTB_REGION_BITS if not using regions for this entry to not interfere with the ones that are using regions

    uint64_t new_region = (ip >> 2 >> _BTB_SET_BITS >> _BTB_TAG_SIZE) & _REGION_MASK;
    region_tag_entry_count[new_region] += require_region;
    if (replaced_entry.has_value() and utilise_regions(replaced_entry.value().target_size) and replaced_entry.value().ip_tag != 0
        and replaced_entry.value().target != 0) {
      uint64_t old_region = (replaced_entry.value().ip_tag >> 2 >> _BTB_SET_BITS >> _BTB_TAG_SIZE) & _REGION_MASK;
      if (region_tag_entry_count[old_region] == 0) {
        // std::cerr << "WARNING: WE TRY REMOVING AN ALREADY 0 VALUE" << std::endl;
        // std::cerr << "OLD REGION: " << old_region << std::endl;
      } else {
        region_tag_entry_count[old_region] -= 1;
        if (region_tag_entry_count[old_region] == 0) {
          region_tag_entry_count.erase(region_tag_entry_count.find(old_region));
        }
      }
    }
    // region_tag_entry_count[new_region] += replaced_entry.has_value();
    uint64_t sum = std::accumulate(std::begin(region_tag_entry_count), std::end(region_tag_entry_count), 0,
                                   [](const auto prev, const auto& elem) { return prev + elem.second; });
    // assert(sum <= BTB_SETS * BTB_WAYS);
  }

  sim_stats.btb_updates++;

  auto diff_tag = std::bitset<64>(ip ^ prev_branch_ip);
  for (size_t idx = 0; idx < 64; idx++) {
    sim_stats.btb_tag_switch_entropy[idx] += diff_tag[idx];
  }

  if (is_static) {
    sim_stats.btb_static_updates++;
    auto ip_bits = std::bitset<64>(ip);
    for (size_t idx = 0; idx < 64; idx++) {
      sim_stats.btb_tag_entropy[idx] += ip_bits[idx];
    }
  }
  if (!warmup) {
    if (sim_stats.max_regions < region_tag_entry_count.size()) {
      sim_stats.max_regions = region_tag_entry_count.size();
    }
    std::vector<std::pair<uint64_t, uint64_t>> sort_vec(region_tag_entry_count.begin(), region_tag_entry_count.end());
    std::sort(sort_vec.begin(), sort_vec.end(), [](auto& a, auto& b) { return a.second > b.second; });
    uint64_t min2ref = 0, sum_count = 0;
    for (auto [tag, count] : sort_vec) {
      // assert(count < BTB_SETS * BTB_WAYS);
      if (tag == 0) {
        continue;
      }
      min2ref += 1;
      sum_count += count;
      if (sum_count > 0.9 * BTB_SETS * BTB_WAYS)
        break;
    }
    if (sim_stats.min_regions < min2ref) {
      sim_stats.min_regions = min2ref;
    }
  }
  /********* STATS END *********/
  /********* UPDATE STATE *********/
  prev_branch_ip = ip;
}
