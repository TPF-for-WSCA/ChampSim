
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

#include "ooo_cpu.h"
// We are only using those in the preprocessor
#define USE_FIFO true
#define USE_SRRIP false

#if USE_FIFO
#include "msl/fifo_table.h"
#elif USE_SRRIP
#include "msl/srrip_table.h"
#endif
#include "msl/lru_table.h"

#define SMALL_BIG_WAY_SPLIT 12 // NOTE: This has a semantic meaning, as in smaller targets reside within the same tag region (for 512 sets)
#define BIGGEST_BTB_X_WAY 25
#define REGION_BTB_FILTER_ENABLED false
#define SAMPLING_DISTANCE 500000

uint64_t invalid_replacements = 0;
/*
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
*/
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

std::map<uint32_t, std::map<uint64_t, uint64_t>> region_tag_entry_count = {};
std::map<uint64_t, uint64_t> total_region_tag_entry_count = {};
std::map<uint64_t, uint16_t> region_count_in_small_btb = {};
std::vector<uint8_t> index_bits;
std::vector<uint8_t> tag_bits;
std::vector<uint8_t> btb_addressing_hash;
std::array<std::set<uint64_t>, 64> observed_entries_per_region_size = {};

bool INSERT_FILTER_VICTIMS = false;
bool wrongpath = false;
std::size_t USE_REGIONALIZED_BTB_OFFSET = 0;
std::size_t _INDEX_MASK = 0;
std::size_t _FILTER_INDEX_MASK = 0;
std::size_t _FILTER_BTB_SET_BITS = 0;
std::size_t _TAG_MASK = 0;
std::size_t _FULL_TAG_MASK = 0;
std::size_t _REGION_MASK = 0;
std::size_t _BTB_SETS = 0;
std::size_t _BTB_WAYS = 0;
uint8_t _BTB_CLIPPED_TAG = 0;
uint8_t _BTB_TAG_SIZE = 0;
uint8_t _BTB_SET_BITS;
uint64_t _BTB_TAG_REGIONS = 0; // This is the number of regions in the region BTB
uint64_t _BTB_TAG_REGION_WAYS = 0;
uint64_t _BTB_TAG_REGION_SETS = 0;
uint64_t _BTB_TAG_REGION_SET_IDX_BITS = 0;
uint8_t _BTB_TAG_REGION_SIZE = 0; // This is the size of a single region in bits
uint64_t _BTB_REGION_BITS = 0;    // This is the number of bits required to assign an ID to all regions in the region BTB (log2(BTB_TAG_REGIONS))
uint64_t last_stats_cycle = 0;
uint64_t _isa_shiftamount = 2;
constexpr std::size_t BTB_INDIRECT_SIZE = 4096;
constexpr std::size_t RAS_SIZE = 64;
constexpr std::size_t CALL_SIZE_TRACKERS = 1024;
bool small_way_regions_enabled = 0;
bool big_way_regions_enabled = 0;
bool _PERFECT_MAPPING = false;

uint64_t prev_branch_ip = 0;
uint64_t prev_branch_tag = 0;
std::map<uint32_t, uint64_t> offset_reuse_freq;
std::set<uint64_t> branch_ip;
std::set<uint32_t> regions_inserted;

// size_t region_btb_insers = 0;

// TODO: Only makes sense with BTB-X
bool utilise_regions(size_t way_size)
{
  if (way_size > BIGGEST_BTB_X_WAY)
    return false;
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
std::map<uint64_t, uint64_t> _reversed;

inline uint64_t reverse_bits(uint64_t a)
{
  auto val = _reversed.find(a);
  if (val != _reversed.end()) {
    return val->second;
  }
  uint64_t rev = 0;
  uint64_t in = a;
  while (a) {
    rev |= (a & 0x1);
    rev <<= 1;
    a >>= 1;
  }
  _reversed[in] = rev;
  return rev;
}

enum BTB_ReplacementStrategy { LRU, REF0, REF };

uint64_t shuffle_ip_tag(uint64_t ip_tag)
{
  if (btb_addressing_hash.empty()) {
    return ip_tag;
  } else {
    std::bitset<64> ip_tag_b{ip_tag};
    std::bitset<64> ip_b{0};
    size_t i = 0;
    for (; i < btb_addressing_hash.size(); i++) {
      ip_b[i] = ip_tag_b[btb_addressing_hash.at(i)];
    }
    for (; i < 64; i++) {
      ip_b[i] = ip_tag_b[i];
    }
    return ip_b.to_ullong();
  }
}

auto get_region(uint64_t ip)
{
  ip = shuffle_ip_tag(ip);
  auto addr = ip >> _isa_shiftamount >> _BTB_SET_BITS >> _BTB_TAG_SIZE;
  uint64_t tag = 0;
  if (_BTB_TAG_REGION_SIZE > 0){
  
    uint64_t upper_addr = addr >> _BTB_TAG_REGION_SIZE;
    upper_addr = reverse_bits(upper_addr);
    const uint64_t a = 1;
    const double r = 1.1f;
    for (uint64_t i = 1; i < _BTB_TAG_REGION_SIZE + 1; i++) {
      uint64_t num_bits = (uint64_t)std::round(a * (1 - std::pow(r, i)) / (1.0 - r)) - 1;
      uint8_t local_bit = addr & 0x1;
      addr >>= 1;
      if (num_bits && upper_addr) {
        uint64_t mask = (1 << num_bits) - 1;
        uint16_t partial_tag = upper_addr & mask;
        upper_addr >>= num_bits;
        while (partial_tag) {
          local_bit = local_bit ^ (partial_tag & 0x1);
          partial_tag >>= 1;
        }
      }
      tag |= (local_bit << i);
    }
    tag &= _REGION_MASK;
  }
  return tag;
}

struct FilterBTBEntry {
  uint64_t ip_tag = 0;
  uint64_t target;
  branch_info type = branch_info::ALWAYS_TAKEN;
  std::tuple<uint16_t, uint16_t, uint64_t> region_idx_tag = {0, 0, 0};
  uint16_t target_size = 64; // TODO: Only update for which we have sizes
  uint64_t offset_mask = -1;

  // TODO: shift indexes and tags into place
  auto index() const
  {
    auto ip = shuffle_ip_tag(ip_tag);
    auto idx = (ip >> _isa_shiftamount) & _FILTER_INDEX_MASK;
    return idx;
  }
  auto tag() const
  {
    auto ip = shuffle_ip_tag(ip_tag);
    auto tag = ip >> _isa_shiftamount >> _FILTER_BTB_SET_BITS;
    return tag;
  }

  auto partial_tag() const
  {
    auto ip = shuffle_ip_tag(ip_tag);
    uint64_t tag = ip >> _isa_shiftamount >> _FILTER_BTB_SET_BITS;
    return tag;
  }

  auto get_prediction() const { return target; }
};

struct BTBEntry {
  uint64_t ip_tag = 0;
  uint64_t target;
  branch_info type = branch_info::ALWAYS_TAKEN;
  // Set / precise / magic pointers
  std::tuple<uint16_t, uint16_t, uint64_t> region_idx_tag = {0, 0, 0};
  uint16_t target_size = 64; // TODO: Only update for which we have sizes
  uint64_t offset_mask = -1;
  uint8_t precise_branch_type;
  bool useless = false;
  bool replacement_protected = false;

  // TODO: shift indexes and tags into place
  auto index() const
  {
    auto ip = shuffle_ip_tag(ip_tag);
    auto idx = (ip >> _isa_shiftamount) & _INDEX_MASK;
    return idx;
  }
  auto tag() const
  {
    uint64_t addr = ip_tag;
    addr = addr >> _isa_shiftamount >> _BTB_SET_BITS;
    if (!_BTB_CLIPPED_TAG) {
      return addr;
    }
    uint64_t upper_addr = addr >> _BTB_TAG_SIZE;
    upper_addr = reverse_bits(upper_addr);
    // _BTB_TAG_SIZE <-- target size
    const uint64_t a = 1;
    const double r = 1.1f;

    uint64_t tag = 0;
    for (uint64_t i = 1; i < _BTB_TAG_SIZE + 1; i++) {
      uint64_t num_bits = (uint64_t)std::round(a * (1 - std::pow(r, i)) / (1.0 - r)) - 1;
      uint8_t local_bit = addr & 0x1;
      addr >>= 1;
      if (num_bits && upper_addr) {
        uint64_t mask = (1 << num_bits) - 1;
        uint16_t partial_tag = upper_addr & mask;
        upper_addr >>= num_bits;
        while (partial_tag) {
          local_bit = local_bit ^ (partial_tag & 0x1);
          partial_tag >>= 1;
        }
      }
      tag |= (local_bit << i);
    }
    if (ip_tag && _BTB_TAG_REGIONS && utilise_regions(target_size)) {
      // TODO: double check if the shift amount of the L2_BTB TAG size is correct and we are not overriding the actual tag bits
      auto masked_bits = tag & (_REGION_MASK << _BTB_TAG_SIZE);
      tag ^= masked_bits;
      // TODO: add third option / replace _PERFECT_MAPPING with enum / right now we have a hard wired to precise pointers
      if (_PERFECT_MAPPING)
        tag |= (std::get<1>(region_idx_tag) << _BTB_TAG_SIZE);
      else
        tag |= (std::get<0>(region_idx_tag) << _BTB_TAG_SIZE); // Precise pointers
    }
    return tag;
  }

  auto partial_tag() const
  {
    auto ip = shuffle_ip_tag(ip_tag);
    uint64_t tag = ip >> _isa_shiftamount >> _BTB_SET_BITS;
    if (!_BTB_CLIPPED_TAG) {
      return tag;
    }

    tag &= _TAG_MASK;
    return tag;
  }

  auto get_prediction() const
  {
    auto offset = target & offset_mask;
    auto prediction = ip_tag & (~offset_mask);
    prediction |= offset;
    // if (target != prediction) {
    //   std::cerr << "Warning: differing prediction from target\n\tPrediction: " << prediction << "\n\tTarget: " << target << std::endl;
    // }
    return prediction;
  }
};

struct region_btb_entry_t {
  uint64_t ip_tag = 0;
  uint64_t max_pointer = 0;
  auto tag() const
  {
    return get_region(ip_tag);
  }
  auto index() const
  {
    auto ip = shuffle_ip_tag(ip_tag);
    // NOTE: If shifted by (_BTB_REGION_BITS - _BTB_SET_BITS) this term results in "big idx" inserts, so the msbs of the region are used for indexing
    uint64_t raw_idx = this->tag() & (_BTB_TAG_REGION_SETS - 1);
    return raw_idx; // NOTE: keep track how many entries we observe per set
  }
  auto partial_tag() const { return 0; }
};

// TODO: Currently not implemented -- Add later on
// extern bool is_kernel(uint64_t ip);
// extern bool is_shared_or_vdso(uint64_t ip);

// TODO: Make fifo/srrip/lru configurable per component -- how to most effectively? I don't know...
#if USE_FIFO
std::map<O3_CPU*, champsim::msl::fifo_table<region_btb_entry_t>> REGION_BTB;
std::map<O3_CPU*, champsim::msl::fifo_table<FilterBTBEntry>> REGION_FILTER_BTB;
#elif USE_SRRIP
std::map<O3_CPU*, champsim::msl::srrip_table<BTBEntry>> BTB;
std::map<O3_CPU*, champsim::msl::srrip_table<region_btb_entry_t>> REGION_BTB;
std::map<O3_CPU*, champsim::msl::srrip_table<FilterBTBEntry>> REGION_FILTER_BTB;
#else
std::map<O3_CPU*, champsim::msl::lru_table<FilterBTBEntry>> REGION_FILTER_BTB;
std::map<O3_CPU*, champsim::msl::lru_table<region_btb_entry_t>> REGION_BTB;
#endif
std::map<O3_CPU*, champsim::msl::lru_table<BTBEntry>> BTB;
// TODO: make sure that the BTBEntry types here are always calculating full tags - might require another type
std::map<O3_CPU*, std::map<uint64_t, uint64_t>> REGION_REF_COUNT;
std::map<O3_CPU*, std::array<uint64_t, BTB_INDIRECT_SIZE>> INDIRECT_BTB;
std::map<O3_CPU*, std::bitset<champsim::lg2(BTB_INDIRECT_SIZE)>> CONDITIONAL_HISTORY;
std::map<O3_CPU*, std::deque<uint64_t>> RAS;
std::map<O3_CPU*, std::deque<uint64_t>> WRONGPATH_BACKUP_RAS;
std::map<O3_CPU*, std::array<uint64_t, CALL_SIZE_TRACKERS>> CALL_SIZE;

} // namespace

void O3_CPU::initialize_btb()
{
  if (intel) {
    _isa_shiftamount = 0;
  }
  std::cout << "BTB INITIALIZED WITH\nFULLY ASSOCIATIVE REGIONS: " << (BTB_TAG_REGION_WAYS == BTB_TAG_REGIONS) << "\nPERFECT MAPPING: " << btb_perfect_mapping
            << ", FILTER BTB: " << REGION_BTB_FILTER_ENABLED << std::endl;
#if USE_SRRIP
  ::BTB.insert({this, champsim::msl::srrip_table<BTBEntry>{BTB_SETS, BTB_WAYS}});
#else
  ::BTB.insert({this, champsim::msl::lru_table<BTBEntry>{BTB_SETS, BTB_WAYS}});
#endif
  USE_REGIONALIZED_BTB_OFFSET = this->BTB_FILTER_BTB_LIMIT;
  INSERT_FILTER_VICTIMS = USE_REGIONALIZED_BTB_OFFSET != 0;
  if (REGION_BTB_FILTER_ENABLED && this->BTB_TAG_REGIONS) {
#if USE_FIFO
    ::REGION_FILTER_BTB.insert({this, champsim::msl::fifo_table<FilterBTBEntry>{BTB_SETS / 16, BTB_WAYS / 2}}); // TODO: How many entries should we really use?
#elif USE_SRRIP
    ::REGION_FILTER_BTB.insert({this, champsim::msl::srrip_table<FilterBTBEntry>{BTB_SETS / 16, BTB_WAYS / 2}}); // TODO: How many entries should we really use?
#else
    ::REGION_FILTER_BTB.insert({this, champsim::msl::lru_table<FilterBTBEntry>{BTB_SETS / 16, BTB_WAYS / 2}}); // TODO: How many entries should we really use?
#endif
    _FILTER_INDEX_MASK = (BTB_SETS / 16) - 1;
    _FILTER_BTB_SET_BITS = champsim::lg2(BTB_SETS / 16);
    ::REGION_REF_COUNT.insert({this, {}});
  }
  _PERFECT_MAPPING = btb_perfect_mapping;
  // TODO: Make region BTB configurable for way/sets
  size_t ways = 1;
  size_t sets = 1;
  if (BTB_TAG_REGIONS) {
    sets = BTB_TAG_REGIONS / BTB_TAG_REGION_WAYS;
    ways = BTB_TAG_REGION_WAYS;
  }
#if USE_FIFO
  ::REGION_BTB.insert({this, champsim::msl::fifo_table<region_btb_entry_t>{sets, ways}});
#elif USE_SRRIP
  ::REGION_BTB.insert({this, champsim::msl::srrip_table<region_btb_entry_t>{sets, ways}});
#else
  ::REGION_BTB.insert({this, champsim::msl::lru_table<region_btb_entry_t>{sets, ways}});
#endif
  std::fill(std::begin(::INDIRECT_BTB[this]), std::end(::INDIRECT_BTB[this]), 0);
  ::btb_addressing_hash = btb_index_tag_hash;
  std::fill(std::begin(::CALL_SIZE[this]), std::end(::CALL_SIZE[this]), 4);
  ::CONDITIONAL_HISTORY[this] = 0;
  _BTB_SET_BITS = champsim::lg2(BTB_SETS);
  if (this->BTB_CLIPPED_TAG) {
    _BTB_CLIPPED_TAG = 1;
    _BTB_TAG_SIZE = this->BTB_TAG_SIZE;
    if (this->BTB_TAG_REGIONS) {
      _BTB_TAG_REGIONS = this->BTB_TAG_REGIONS;
      _BTB_TAG_REGION_WAYS = this->BTB_TAG_REGION_WAYS;
      _BTB_TAG_REGION_SETS = this->BTB_TAG_REGIONS / this->BTB_TAG_REGION_WAYS;
      for (uint16_t i = 0; i < _BTB_TAG_REGION_SETS; i++) {
        sim_stats.region_btb_inserts_per_set.insert({i, 0});
      }
      _BTB_TAG_REGION_SET_IDX_BITS = champsim::lg2(_BTB_TAG_REGION_SETS);
      _BTB_TAG_REGION_SIZE =
          (this->BTB_TAG_REGIONS) ? this->BTB_TAG_REGION_SIZE : 0; // (this->BTB_TAG_REGIONS) ? this->BTB_TAG_REGION_SIZE + _BTB_TAG_REGION_SET_IDX_BITS : 0;
      _REGION_MASK = _BTB_TAG_REGIONS ? (pow2(_BTB_TAG_REGION_SIZE) - 1) : (pow2(62 - _BTB_SET_BITS - _BTB_TAG_SIZE) - 1);
      _BTB_REGION_BITS = champsim::lg2(_BTB_TAG_REGIONS);
    }
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
        auto offset_size = (*way_sizes_begin) + 2; // NOTE: We add 2 here to not need to shift in and out 2 0 bits in the prediction
        if (offset_size > 64) {
          offset_size = 64;
        }
        set_begin->data.offset_mask = pow2(offset_size) - 1;
        way_sizes_begin++;
      }
    }
  }
}

// __attribute__((optimize(0)))
std::tuple<uint64_t, uint64_t, uint8_t, uint8_t> O3_CPU::btb_prediction(uint64_t ip)
{
  // TODO: add if condition with breaking condition
  // if (!warmup && ip == 18446462598868070740 && current_cycle >= 7113112) {
  //   std::cout << "this is one of the faulting branches" << std::endl;
  // }
  std::optional<::BTBEntry> btb_entry = std::nullopt;
  std::optional<::FilterBTBEntry> filter_hit = std::nullopt;
  if (REGION_BTB_FILTER_ENABLED && _BTB_TAG_REGIONS)
    filter_hit = ::REGION_FILTER_BTB.at(this).check_hit({ip});
  if (_BTB_TAG_REGIONS && !filter_hit.has_value()) {
    auto region_idx_ = ::REGION_BTB.at(this).check_hit_idx({ip});
    std::optional<::BTBEntry> partial = std::nullopt;
    if (BTB_PARTIAL_TAG_RESOLUTION) {
      partial = ::BTB.at(this).check_hit({ip, 0, ::branch_info::ALWAYS_TAKEN, std::tuple<uint16_t, uint16_t, uint64_t>{0, 0, 0}}, true);
    }
    std::optional<::BTBEntry> full_small = std::nullopt;
    std::optional<::BTBEntry> partial_small = std::nullopt;
    std::optional<::BTBEntry> full_big = std::nullopt;
    std::optional<::BTBEntry> partial_big = std::nullopt;
    std::optional<::BTBEntry> full_64 = std::nullopt;
    if (small_way_regions_enabled && region_idx_.has_value()) {
      full_small = ::BTB.at(this).check_hit({ip, 0, branch_info::ALWAYS_TAKEN, region_idx_.value(), 0});
      if (full_small.has_value() && !utilise_regions(full_small.value().target_size)) {
        full_small = std::nullopt;
      }
    } else if (!small_way_regions_enabled) {
      partial_small = ::BTB.at(this).check_hit({ip, 0, branch_info::ALWAYS_TAKEN, std::tuple<uint16_t, uint16_t, uint64_t>{(uint16_t)-1, 0, 0}, 0});
    }
    if (big_way_regions_enabled && region_idx_.has_value()) {
      full_big = ::BTB.at(this).check_hit({ip, 0, branch_info::ALWAYS_TAKEN, region_idx_.value(), BTB_TARGET_SIZES.end()[-2]});
      if (full_big.has_value() && !utilise_regions(full_big.value().target_size)) {
        full_small = std::nullopt;
      }
    } else if (!big_way_regions_enabled) {
      partial_big = ::BTB.at(this).check_hit(
          {ip, 0, branch_info::ALWAYS_TAKEN, std::tuple<uint16_t, uint16_t, uint64_t>{(uint16_t)-1, 0, 0}, BTB_TARGET_SIZES.end()[-2]});
    }
    full_64 = ::BTB.at(this).check_hit({ip, 0, branch_info::ALWAYS_TAKEN, std::tuple<uint16_t, uint16_t, uint64_t>{(uint16_t)-1, 0, 0}, 64});
    if (full_64.has_value() && full_64.value().target_size != 64) {
      full_64 = std::nullopt; // fixing up for when we are using perfect matching, as in this case we will alias as we use the actual region bits instead of
                              // their index
    }

    assert(
        !(full_small.has_value() && full_big.has_value()
          && full_small.value().ip_tag != full_big.value().ip_tag)); // This should never happen as then we should have updated the value instead of re-inserted
    if (full_small.has_value()) {
      btb_entry = full_small.value();
    } else if (full_big.has_value()) {
      btb_entry = full_big.value();
    } else if (partial_small.has_value()) {
      btb_entry = partial_small.value();
    } else if (partial_big.has_value()) {
      btb_entry = partial_big.value();
    } else if (partial.has_value()) { // could only ever be true if partial resolution is enabled
      btb_entry = partial.value();
    } else if (full_64.has_value()) {
      btb_entry = full_64.value();
    }
  } else if (!filter_hit.has_value()) {
    btb_entry = ::BTB.at(this).check_hit({ip, 0, ::branch_info::ALWAYS_TAKEN, std::tuple<uint16_t, uint16_t, uint64_t>{0, 0, 0}, 0});
  } else {
    auto v = filter_hit.value();
    btb_entry = {v.ip_tag, v.target, v.type, v.region_idx_tag};
  }

  // no prediction for this IP
  // default: no aliasing, thus returning ip itself as recorded ip
  if (!btb_entry.has_value())
    return {0, ip, false, NOT_BRANCH};

  auto lras = (!wrongpath) ? &RAS : &WRONGPATH_BACKUP_RAS;
  if (btb_entry->type == ::branch_info::RETURN) {
    if (std::empty((*lras)[this]))
      return {0, btb_entry->ip_tag, true, BRANCH_RETURN};

    // peek at the top of the RAS and adjust for the size of the call instr
    auto target = (*lras)[this].back();
    auto size = ::CALL_SIZE[this][target % std::size(::CALL_SIZE[this])];

    return {target + 4, btb_entry->ip_tag, true, BRANCH_RETURN}; // assume fixed size for now
  }
  if (wrongpath && (btb_entry->precise_branch_type == BRANCH_DIRECT_CALL || btb_entry->precise_branch_type == BRANCH_INDIRECT_CALL)) {
    // modify the wrongpath ras
    (*lras)[this].push_back(ip);
    if (std::size((*lras)[this]) > RAS_SIZE)
      (*lras)[this].pop_front();
  }
  /*
    if (btb_entry->type == ::branch_info::INDIRECT) {
      auto hash = (ip >> isa_shiftamount) ^ ::CONDITIONAL_HISTORY[this].to_ullong();
      return {::INDIRECT_BTB[this][hash % std::size(::INDIRECT_BTB[this])], btb_entry->ip_tag, true};
    }*/

  return {btb_entry->get_prediction(), btb_entry->ip_tag, btb_entry->type != ::branch_info::CONDITIONAL, btb_entry->precise_branch_type};
}

// TODO: ONLY UPDATE WHEN FITTING IN THE WAY
// __attribute__((optimize(0)))
void O3_CPU::update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  uint64_t new_region = get_region(ip);

  uint64_t offset_size = (ip >> isa_shiftamount) ^ (branch_target >> isa_shiftamount);
  uint8_t num_bits = 0;
  while (offset_size) {
    offset_size >>= 1;
    num_bits++;
  }

  // THIS IS AN APPLICATION PROPERTY -- WE SHOULD MEASURE ALSO THE HW PROPERTY AND COMPARE THE TWO (at prediction time)
  if (taken && prev_branch_tag != new_region) {
    sim_stats.btb_region_switching_dynamic++;
    prev_branch_tag = new_region;
  }

  // TODO: Integrate with sim
  bool is_static = branch_ip.insert(ip).second;
  sim_stats.static_branch_count += is_static;
  sim_stats.dynamic_branch_count += 1;
  if (branch_target) { // static and dynamic is only branches inserted into the BTB -- not any branch
    for (int j = 0; j < 64; j++) {
      auto region_id = ip >> j;
      if (region_id & 0x1) {
        sim_stats.static_bit_counts[j] += is_static;
        sim_stats.dynamic_bit_counts[j] += 1;
      }
      bool first_observation = observed_entries_per_region_size[j].insert(region_id).second;
      sim_stats.max_regions_per_region_size[j] += first_observation;
    }
  }

  // no need to check for wrongpath: we do only update on commit, so no wrongpath will enter here
  // add something to the RAS
  if (branch_type == BRANCH_DIRECT_CALL || branch_type == BRANCH_INDIRECT_CALL) {
    RAS[this].push_back(ip);
    if (std::size(RAS[this]) > RAS_SIZE)
      RAS[this].pop_front();
  }

  // COMMON STATS
  if (branch_target) {
    sim_stats.btb_updates++;
    // if (branch_type != NOT_BRANCH)
    //   sim_stats.branch_ip_set.insert((ip >> isa_shiftamount >> _BTB_SET_BITS));

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
  }
  // END COMMON STATS

  // updates for indirect branches
  // if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL)) {
  //   auto hash = (ip >> isa_shiftamount) ^ ::CONDITIONAL_HISTORY[this].to_ullong();
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

  std::optional<std::tuple<uint16_t, uint16_t, uint64_t>> region_idx = std::nullopt;
  std::optional<::BTBEntry> opt_entry;
  uint8_t entry_size = num_bits;
  // TODO: ADD REGION INFORMATION IF AVAILABLE
  std::optional<std::tuple<uint16_t, uint16_t, uint64_t>> tmp_region_idx =
      (small_way_regions_enabled || big_way_regions_enabled) ? ::REGION_BTB.at(this).check_hit_idx({ip}) : std::nullopt;

  if (REGION_BTB_FILTER_ENABLED && _BTB_TAG_REGIONS) {
    auto filter_hit = ::REGION_FILTER_BTB.at(this).check_hit({ip});
    if ((filter_hit.has_value() || (!tmp_region_idx.has_value() && REGION_REF_COUNT.at(this)[new_region] < USE_REGIONALIZED_BTB_OFFSET))) {
      if (!filter_hit.has_value()) {
        REGION_REF_COUNT.at(this)[new_region]++;
      } else if (!branch_target) {
        branch_target = filter_hit.value().target; // This ensures we are not removing the target from a not taken branch
      }
      auto replaced = ::REGION_FILTER_BTB.at(this).fill(
          {ip, branch_target, type, {0, 0, new_region}}, &sim_stats); // TODO: add element, only if we cross threshold insert into region and add future branches there and
                                                          // only when replaced from filter btb add to big btb
      bool valid_replacement = replaced.has_value() && replaced.value().ip_tag && replaced.value().ip_tag != ip;
      if (valid_replacement) { // if iptag is 0 its an invalid(ated) entry
        uint64_t old_region = get_region(replaced.value().ip_tag);
        assert(REGION_REF_COUNT.at(this)[old_region]);
        REGION_REF_COUNT.at(this)[old_region]--;
        new_region = old_region;
        tmp_region_idx = ::REGION_BTB.at(this).check_hit_idx({replaced.value().ip_tag});
      }
      // TODO: Do insert if already in regionalized btb / do insert if not in regionalized btb but we are right at the boundary / do not insert every
      // replacement
      if (valid_replacement && INSERT_FILTER_VICTIMS
          && (REGION_REF_COUNT.at(this)[new_region] >= USE_REGIONALIZED_BTB_OFFSET - 1 || tmp_region_idx.has_value())) { // try always insert
        auto v = replaced.value();
        ip = v.ip_tag;
        type = v.type;
        branch_target = v.get_prediction();
        tmp_region_idx = (small_way_regions_enabled || big_way_regions_enabled) ? ::REGION_BTB.at(this).check_hit_idx({ip}) : std::nullopt;

      } else {
        // if (valid_replacement) {
        //   std::cout << "Getting rid for good" << std::endl;
        // }
        return;
      }
    }
  }
  //{ NOTE: VERY VERBOSE DEBUGGING OUTPUT BELOW
  // std::cout << "Current Regions in Region BTB:" << std::endl;
  // for (auto it = ::REGION_BTB.at(this).begin(); it != ::REGION_BTB.at(this).end(); it++) {
  //   if (it->last_used == 0)
  //     continue;
  //   std::cout << "\tBlock" << it - ::REGION_BTB.at(this).begin() << ": " << it->data.tag() << std::endl;
  // }
  // std::cout << "Number of Blocks in Ways: " << std::endl;
  // std::map<size_t, size_t> count_per_way{};
  // for (auto it = ::BTB.at(this).begin(); it != ::BTB.at(this).end(); it++) {
  //   if (it->last_used == 0)
  //     continue;
  //   count_per_way[it->data.target_size]++;
  // }
  // for (auto const& [way_size, count] : count_per_way) {
  //   std::cout << "\tWay " << way_size << "\tCount: " << count << std::endl;
  // }
  //}

  std::optional<::BTBEntry> small_hit = std::nullopt;
  std::optional<::BTBEntry> big_hit = std::nullopt;
  std::optional<::BTBEntry> hit_64 = std::nullopt;
  std::optional<::BTBEntry> lru_elem = std::nullopt;

  if (small_way_regions_enabled && tmp_region_idx.has_value()) {
    small_hit = ::BTB.at(this).check_hit({ip, 0, type, tmp_region_idx.value(), 0});
    if (small_hit.has_value() && !utilise_regions(small_hit.value().target_size)) {
      small_hit = std::nullopt;
    }
  } else if (!small_way_regions_enabled) {
    small_hit = ::BTB.at(this).check_hit({ip, 0, type, std::tuple<uint16_t, uint16_t, uint64_t>{(uint16_t)-1, 0, 0}, 0});
  }
  if (big_way_regions_enabled && tmp_region_idx.has_value()) {
    big_hit = ::BTB.at(this).check_hit({ip, 0, type, tmp_region_idx.value(), BTB_TARGET_SIZES.end()[-2]});
    if (big_hit.has_value() && !utilise_regions(big_hit.value().target_size)) {
      big_hit = std::nullopt;
    }
  } else if (!big_way_regions_enabled) {
    big_hit = ::BTB.at(this).check_hit({ip, 0, type, std::tuple<uint16_t, uint16_t, uint64_t>{(uint16_t)-1, 0, 0}, BTB_TARGET_SIZES.end()[-2]});
  }
  hit_64 = ::BTB.at(this).check_hit({ip, 0, type, std::tuple<uint16_t, uint16_t, uint64_t>{(uint16_t)-1, 0, 0}, 64});
  if (hit_64.has_value() && hit_64.value().target_size != 64) {
    hit_64 =
        std::nullopt; // fixing up for when we are using perfect matching, as in this case we will alias as we use the actual region bits instead of their index
  }
  if (!small_hit.has_value() && !big_hit.has_value() && !hit_64.has_value()) {
    lru_elem = ::BTB.at(this).get_replacement_element(::BTBEntry{ip, 0}, num_bits);
  }

  assert(!(small_hit.has_value() && big_hit.has_value()) || (small_hit.value().ip_tag == big_hit.value().ip_tag));

  // TODO: pass way size and not num bits to utilise regions function here
  // TODO: check partial and check if utilise region is true -> update that value
  // TODO: get rid of hits that have the wrong size - those need to be updated and inserted into a larger way
  bool small_region = small_hit.has_value() && num_bits <= small_hit.value().target_size && utilise_regions(small_hit.value().target_size);
  bool big_region = big_hit.has_value() && num_bits <= big_hit.value().target_size && utilise_regions(big_hit.value().target_size);
  bool could_require_region = utilise_regions(entry_size);
  bool require_region = small_region || big_region || could_require_region;
  // || (lru_region && (!small_hit.has_value() || !small_way_regions_enabled) && (!big_hit.has_value() || !big_way_regions_enabled))
  //|| (tmp_region_idx.has_value()
  //   && ((small_hit.has_value() && small_way_regions_enabled) || (big_hit.has_value() && big_way_regions_enabled))); // add if we have a hit
  if (require_region) {
    region_idx = ::REGION_BTB.at(this).check_hit_idx({ip});
    std::optional<::region_btb_entry_t> replaced = std::nullopt;
    bool insert = false;
    if (!region_idx.has_value() && BTB_PARTIAL_TAG_RESOLUTION) {
      opt_entry = ::BTB.at(this).check_hit({ip, branch_target, type, std::tuple<uint16_t, uint16_t, uint64_t>{0, 0, 0}}, true);
      if (!opt_entry.has_value() || (opt_entry.has_value() && opt_entry.value().get_prediction() != branch_target)) {
        // TODO: these asserts are only ok if we have as many regions as 2**region_bits - add guard for that
        // auto rv = regions_inserted.insert(::region_btb_entry_t{ip}.tag());
        // assert(rv.second);
        auto elem = ::region_btb_entry_t{ip};
        sim_stats.big_region_small_region_mapping[elem.index()].insert(elem.tag());
        replaced = ::REGION_BTB.at(this).fill(elem, &sim_stats);
        sim_stats.branch_tag_set.insert(elem.tag());
        insert = true;
        // region_btb_insers++;
        // assert(!replaced_element.has_value() || replaced_element.value().ip_tag == 0);
        region_idx = ::REGION_BTB.at(this).check_hit_idx({ip});
      }
    } else if (!region_idx.has_value()) {
      // auto rv = regions_inserted.insert(::region_btb_entry_t{ip}.tag());
      // assert(rv.second);
      auto elem = ::region_btb_entry_t{ip};
      sim_stats.big_region_small_region_mapping[elem.index()].insert(elem.tag());
      replaced = ::REGION_BTB.at(this).fill(elem, &sim_stats);
      sim_stats.branch_tag_set.insert(elem.tag());
      insert = true;
      // region_btb_insers++;
      // assert(!replaced_element.has_value() || replaced_element.value().ip_tag == 0);
      region_idx = ::REGION_BTB.at(this).check_hit_idx({ip});
    }
    if (insert) {
      if (!replaced.has_value() || replaced.value().ip_tag == 0 || get_region(ip) != get_region(replaced.value().ip_tag)) {
        sim_stats.region_btb_inserts_per_set.at(::region_btb_entry_t{ip}.index())++;
      }
      if (replaced.has_value() && replaced.value().ip_tag != 0 && get_region(ip) != get_region(replaced.value().ip_tag)) {
        sim_stats.region_btb_conflicts++;
        sim_stats.max_region_pointer_sum += replaced.value().max_pointer;
        sim_stats.region_pointer_max_stats[replaced.value().max_pointer]++;
      }
    }
    // assert(region_btb_insers <= 256);
  }
  // else {
  //   if (small_hit.has_value()) {
  //     entry_size = std::max(entry_size, small_hit.value().target_size);
  //   }
  //   if (big_hit.has_value()) {
  //     entry_size = std::max(entry_size, big_hit.value().target_size);
  //   }
  //   if (!small_hit.has_value() && !big_hit.has_value()) {
  //     entry_size = lru_elem.target_size;
  //   }
  // }
  if (small_hit.has_value()) {
    entry_size = std::max(entry_size, small_hit.value().target_size);
    opt_entry = small_hit.value();
  } else if (big_hit.has_value()) {
    entry_size = std::max(entry_size, big_hit.value().target_size);
    opt_entry = big_hit.value();
  } else if (hit_64.has_value()) {
    entry_size = std::max(entry_size, hit_64.value().target_size);
    opt_entry = hit_64.value();
  }

  /********* STATS ACCOUNTING *********/
  // TODO: Only update if prediction is wrong
  std::optional<::BTBEntry> replaced_entry = std::nullopt;
  std::optional<::BTBEntry> invalidated_entry = std::nullopt;
  if (branch_target != 0) {
    uint64_t invalidated_region = 0;
    // Mark entry invalid if we moved it to a bigger target
    if (branch_type != BRANCH_RETURN && opt_entry.has_value() && opt_entry.value().get_prediction() != branch_target) {
      invalidated_entry = ::BTB.at(this).invalidate(opt_entry.value());
      if (invalidated_entry.has_value() && invalidated_entry.value().ip_tag && utilise_regions(invalidated_entry.value().target_size)) {
        invalidated_region = get_region(invalidated_entry.value().ip_tag);
        region_tag_entry_count[invalidated_entry.value().target_size][invalidated_region] -= 1;
        total_region_tag_entry_count[invalidated_region] -= 1;
      }
    }

    // TODO: Check if (since we already know about region or not region) should make two distinct calls out of the below
    auto fill_entry = opt_entry.value_or(
        ::BTBEntry{ip, branch_target, type, region_idx.value_or(std::tuple<uint16_t, uint16_t, uint64_t>{pow2(_BTB_REGION_BITS), 0, 0}), entry_size});
    fill_entry.target = branch_target;
    fill_entry.ip_tag = ip;
    fill_entry.type = type;
    fill_entry.precise_branch_type = branch_type;
    replaced_entry = ::BTB.at(this).fill(
        fill_entry,
        entry_size); // ASSIGN to region 2^BTB_REGION_BITS if not using regions for this entry to not interfere with the ones that are using regions
    uint64_t old_region = 0;
    sim_stats.regions_inserted_per_way[replaced_entry.value().target_size].insert(new_region);
    if (replaced_entry.value().ip_tag != 0 && sim_stats.region_pointer_count[get_region(replaced_entry.value().ip_tag)])
      sim_stats.region_pointer_count[get_region(replaced_entry.value().ip_tag)]--;
    if (region_idx.has_value()) {
      sim_stats.region_pointer_count[get_region(fill_entry.ip_tag)]++;
      auto region_elem = ::REGION_BTB.at(this).begin();
      std::advance(region_elem, std::get<1>(region_idx.value()));
      // if we achieved a new max count of region pointers on this entry increase here
      if (region_elem->data.max_pointer < sim_stats.region_pointer_count[get_region(fill_entry.ip_tag)]) {
        region_elem->data.max_pointer = sim_stats.region_pointer_count[get_region(fill_entry.ip_tag)];
      }
    }

    if (utilise_regions(replaced_entry.value().target_size)) {
      region_tag_entry_count[replaced_entry.value().target_size][new_region] += 1;
      total_region_tag_entry_count[new_region] += 1;
      if (replaced_entry.has_value() && replaced_entry.value().ip_tag) {
        old_region = get_region(replaced_entry.value().ip_tag);
        if (total_region_tag_entry_count[old_region] == 0) {
          // std::cerr << "WARNING: WE TRY REMOVING AN ALREADY 0 VALUE" << std::endl;
          // std::cerr << "OLD REGION: " << old_region << std::endl;
          // std::cerr << "INSTRUCTION TO BLAME: " << std::endl;
          // std::cerr << "\tip: " << ip << ", cycle: " << current_cycle << std::endl;
          total_region_tag_entry_count.erase(total_region_tag_entry_count.find(old_region));
        } else {
          total_region_tag_entry_count[old_region] -= 1;
          if (total_region_tag_entry_count[old_region] == 0) {
            total_region_tag_entry_count.erase(total_region_tag_entry_count.find(old_region));
          }
        }
        if (region_tag_entry_count[replaced_entry.value().target_size][old_region] == 0) {
          // std::cerr << "WARNING: WE TRY REMOVING AN ALREADY 0 VALUE" << std::endl;
          // std::cerr << "OLD REGION: " << old_region << std::endl;
          // std::cerr << "INSTRUCTION TO BLAME: " << std::endl;
          // std::cerr << "\tip: " << ip << ", cycle: " << current_cycle << std::endl;
          region_tag_entry_count[replaced_entry.value().target_size].erase(region_tag_entry_count[replaced_entry.value().target_size].find(old_region));
        } else {
          region_tag_entry_count[replaced_entry.value().target_size][old_region] -= 1;
          if (region_tag_entry_count[replaced_entry.value().target_size][old_region] == 0) {
            region_tag_entry_count[replaced_entry.value().target_size].erase(region_tag_entry_count[replaced_entry.value().target_size].find(old_region));
          }
        }
      }
    }

    // DEBUG SUMS TO FIND EXACT PLACE WE GO WRONG
    // uint64_t sum = std::accumulate(std::begin(total_region_tag_entry_count), std::end(total_region_tag_entry_count), 0,
    //                                [](const auto prev, const auto& elem) { return prev + elem.second; });
    // uint64_t total_blocks = 0;
    // std::map<uint64_t, uint64_t> control_region_tag_mapping;
    // for (auto it = BTB.at(this).begin(); it != BTB.at(this).end(); it++) {
    //   if (it->data.ip_tag
    //       && utilise_regions(it->data.target_size)) { // ignore REGION_BTB.at(this).check_hit({it->data.ip_tag}) as we do not remove/invalidate those
    //     total_blocks++;
    //     control_region_tag_mapping[get_region(it->data.ip_tag)]++;
    //   }
    // }
    // bool problem = false;
    // for (auto const [region, count] : control_region_tag_mapping) {
    //   if (count != total_region_tag_entry_count[region]) {
    //     std::cout << "problem" << std::endl;
    //     problem = true;
    //   }
    // }
    // assert(!problem);
    // assert(sum == total_blocks);
  }

  if (!warmup && SAMPLING_DISTANCE < current_cycle - last_stats_cycle) {
    std::map<uint8_t, std::tuple<uint64_t, uint64_t, uint64_t, uint64_t, uint64_t>> stats_entry{};
    std::map<uint64_t, std::set<uint64_t>> regions_per_way;
    for (auto const& [size, region_count] : region_tag_entry_count) {
      if (sim_stats.max_regions < region_count.size()) {
        sim_stats.max_regions = std::count_if(region_count.begin(), region_count.end(), [](auto pair) { return pair.second; }); // TODO: Filter 0 entries
      }
      std::vector<std::pair<uint64_t, uint64_t>> sort_vec(region_count.begin(), region_count.end());
      std::sort(sort_vec.begin(), sort_vec.end(), [](auto& a, auto& b) { return a.second > b.second; });
      uint64_t min2ref = 0, sum_count = 0;
      // TODO: count current valid entries in btb
      // TODO: Track 90, 95, 99, 99.5% and add to queue whenever we sample

      // std::map<uint8_t, std::map<uint64_t, uint64_t>> region_count_control = {};
      uint64_t total_blocks = 0;
      for (auto it = BTB.at(this).begin(); it != BTB.at(this).end(); it++) {
        if (it->data.ip_tag && utilise_regions(it->data.target_size)
            && size == it->data.target_size) { // REGION_BTB.at(this).check_hit({it->data.ip_tag}) not used as we might have stale
                                               // entries that were covered by regions = we want to know how many we would have needed
          total_blocks++;
          regions_per_way[size].insert(get_region(it->data.ip_tag));

          // auto region = (it->data.ip_tag >> isa_shiftamount >> _BTB_SET_BITS >> _BTB_TAG_SIZE) & _REGION_MASK;
          // region_count_control[it->data.target_size][region]++;
        }
      }
      // TODO: Debug only, remove afterwards / comment out
      // for (auto const& [way_size, cnt_per_size] : region_count_control) {
      //   for (auto const& [region_size, cnt] : cnt_per_size) {
      //     if (region_tag_entry_count[way_size][region_size] != cnt) {
      //       std::cout << "mismatch in region " << region_size << " in way " << way_size << " current cycle: " << current_cycle << std::endl;
      //       std::cerr << "INSTRUCTION TO BLAME: " << std::endl;
      //       std::cerr << "\tip: " << ip << ", cycle: " << current_cycle << std::endl;
      //     }
      //     assert(region_tag_entry_count[way_size][region_size] == cnt);
      //     if (way_size == 64) {
      //       assert(region_tag_entry_count[64][region_size] == 0);
      //     }
      //   }
      // }

      // for (auto [tag, count] : sort_vec) {
      //   total_blocks += count;
      // }

      for (auto [tag, count] : sort_vec) {
        // assert(count <= total_blocks);
        min2ref += (count != 0);
        sum_count += count;
        if (std::get<3>(stats_entry[size]) == 0 && sum_count > 0.995 * total_blocks) {
          std::get<3>(stats_entry[size]) = min2ref;
        }
        if (std::get<2>(stats_entry[size]) == 0 && sum_count > 0.99 * total_blocks) {
          std::get<2>(stats_entry[size]) = min2ref;
        }
        if (std::get<1>(stats_entry[size]) == 0 && sum_count > 0.95 * total_blocks) {
          std::get<1>(stats_entry[size]) = min2ref;
        }
        if (std::get<0>(stats_entry[size]) == 0 && sum_count > 0.9 * total_blocks) {
          std::get<0>(stats_entry[size]) = min2ref;
        }
      }
      if (sim_stats.min_regions < min2ref) {
        sim_stats.min_regions = min2ref;
      }
      std::get<4>(stats_entry[size]) = min2ref;
    }
    sim_stats.region_history.push_back(stats_entry);

    std::map<uint64_t, uint64_t> region_count_sample;
    std::set<uint64_t> combined_set;
    for (auto const [way, set] : regions_per_way) {
      region_count_sample[way] = set.size();
      combined_set.insert(set.begin(), set.end());
    }
    sim_stats.regions_per_way_samples.push_back(region_count_sample);
    sim_stats.region_count_samples.push_back(combined_set.size());
    for (auto it = sim_stats.region_pointer_count.begin(); it != sim_stats.region_pointer_count.end(); it++) {
      if (!it->second) {
        continue;
      }
      sim_stats.region_pointer_cycle_probe_stats[it->second]++;
    }
    last_stats_cycle = current_cycle;
  }
  /********* STATS END *********/
  /********* UPDATE STATE *********/
  prev_branch_ip = ip;
}

void O3_CPU::btb_end_phase(unsigned finished_cpu)
{
  // TODO: go through all entries in the REGION BTB and read out max pointer values
  for (auto it = ::REGION_BTB.at(this).begin(); it != ::REGION_BTB.at(this).end(); it++) {
    sim_stats.region_pointer_max_stats[it->data.max_pointer]++;
  }
}

void O3_CPU::btb_begin_wrongpath()
{
  WRONGPATH_BACKUP_RAS = std::map<O3_CPU*, std::deque<uint64_t>>(RAS);
  wrongpath = true;
}

void O3_CPU::btb_end_wrongpath() { wrongpath = false; }

void O3_CPU::btb_invalidate_entry(uint64_t ip)
{
  if (!btb_invalidate_entry_on_alias && !btb_invalidate_region)
    return;
  std::optional<::BTBEntry> btb_entry = std::nullopt;
  std::optional<::FilterBTBEntry> filter_hit = std::nullopt;
  if (REGION_BTB_FILTER_ENABLED && _BTB_TAG_REGIONS)
    filter_hit = ::REGION_FILTER_BTB.at(this).check_hit({ip});
  std::optional<std::tuple<uint16_t, uint16_t, uint64_t>> region_idx_ = std::nullopt;
  std::optional<::region_btb_entry_t> region_entry = std::nullopt;
  if (_BTB_TAG_REGIONS && !filter_hit.has_value()) {
    region_idx_ = ::REGION_BTB.at(this).check_hit_idx({ip});
    region_entry = ::REGION_BTB.at(this).check_hit({ip});
    std::optional<::BTBEntry> partial = std::nullopt;
    if (BTB_PARTIAL_TAG_RESOLUTION) {
      partial = ::BTB.at(this).check_hit({ip, 0, ::branch_info::ALWAYS_TAKEN, std::tuple<uint16_t, uint16_t, uint64_t>{0, 0, 0}}, true);
    }
    std::optional<::BTBEntry> full_small = std::nullopt;
    std::optional<::BTBEntry> partial_small = std::nullopt;
    std::optional<::BTBEntry> full_big = std::nullopt;
    std::optional<::BTBEntry> partial_big = std::nullopt;
    std::optional<::BTBEntry> full_64 = std::nullopt;
    if (small_way_regions_enabled && region_idx_.has_value()) {
      full_small = ::BTB.at(this).check_hit({ip, 0, branch_info::ALWAYS_TAKEN, region_idx_.value(), 0});
      if (full_small.has_value() && !utilise_regions(full_small.value().target_size)) {
        full_small = std::nullopt;
      }
    } else if (!small_way_regions_enabled) {
      partial_small = ::BTB.at(this).check_hit({ip, 0, branch_info::ALWAYS_TAKEN, std::tuple<uint16_t, uint16_t, uint64_t>{(uint16_t)-1, 0, 0}, 0});
    }
    if (big_way_regions_enabled && region_idx_.has_value()) {
      full_big = ::BTB.at(this).check_hit({ip, 0, branch_info::ALWAYS_TAKEN, region_idx_.value(), BTB_TARGET_SIZES.end()[-2]});
      if (full_big.has_value() && !utilise_regions(full_big.value().target_size)) {
        full_small = std::nullopt;
      }
    } else if (!big_way_regions_enabled) {
      partial_big = ::BTB.at(this).check_hit(
          {ip, 0, branch_info::ALWAYS_TAKEN, std::tuple<uint16_t, uint16_t, uint64_t>{(uint16_t)-1, 0, 0}, BTB_TARGET_SIZES.end()[-2]});
    }
    full_64 = ::BTB.at(this).check_hit({ip, 0, branch_info::ALWAYS_TAKEN, std::tuple<uint16_t, uint16_t, uint64_t>{(uint16_t)-1, 0, 0}, 64});
    if (full_64.has_value() && full_64.value().target_size != 64) {
      full_64 = std::nullopt; // fixing up for when we are using perfect matching, as in this case we will alias as we use the actual region bits instead of
                              // their index
    }

    assert(
        !(full_small.has_value() && full_big.has_value()
          && full_small.value().ip_tag != full_big.value().ip_tag)); // This should never happen as then we should have updated the value instead of re-inserted
    if (full_small.has_value()) {
      btb_entry = full_small.value();
    } else if (full_big.has_value()) {
      btb_entry = full_big.value();
    } else if (partial_small.has_value()) {
      btb_entry = partial_small.value();
    } else if (partial_big.has_value()) {
      btb_entry = partial_big.value();
    } else if (partial.has_value()) { // could only ever be true if partial resolution is enabled
      btb_entry = partial.value();
    } else if (full_64.has_value()) {
      btb_entry = full_64.value();
    }
  } else if (!filter_hit.has_value()) {
    // TODO: Fix to only invalidate in the if condition, never else
    btb_entry = ::BTB.at(this).check_hit({ip, 0, ::branch_info::ALWAYS_TAKEN, std::tuple<uint16_t, uint16_t, uint64_t>{0, 0, 0}, 0});
  } else {
    auto v = filter_hit.value();
    btb_entry = {v.ip_tag, v.target, v.type, v.region_idx_tag};
  }

  // no prediction for this IP
  // default: no aliasing, thus returning ip itself as recorded ip
  if (!btb_entry.has_value() || !region_idx_.has_value()) {
    std::cerr << "WE HAVE NOT FOUND THE ALIASING ENTRY FOR " << ip << std::endl;
    std::cerr << "ALREADY REPLACED?" << std::endl;
    return;
    // assert(0);
  }
  if (btb_entry.value().ip_tag == ip) {
    return; // we already updated the entry in simulation -- evicting it now would result in a double penalty - updating and then throwing away the just updated
            // entry
  }
  if (btb_invalidate_entry_on_alias)
    ::BTB.at(this).invalidate(btb_entry.value());
  else if (btb_invalidate_region)
    ::REGION_BTB.at(this).invalidate(region_entry.value());
}