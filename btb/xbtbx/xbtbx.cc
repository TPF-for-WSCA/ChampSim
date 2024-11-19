
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
#include <set>

#include "msl/lru_table.h"
#include "ooo_cpu.h"

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

std::map<uint64_t, uint64_t> region_tag_entry_count;
std::size_t _INDEX_MASK = 0;
std::size_t _TAG_MASK = 0;
std::size_t _REGION_MASK = 0;
uint8_t _BTB_CLIPPED_TAG = 0;
uint8_t _BTB_TAG_SIZE = 0;
uint8_t _BTB_SET_BITS;
uint16_t _BTB_TAG_REGIONS = 0;
uint8_t _BTB_TAG_REGION_SIZE = 0;
uint16_t _BTB_REGION_BITS = 0;
constexpr std::size_t BTB_INDIRECT_SIZE = 4096;
constexpr std::size_t RAS_SIZE = 64;
constexpr std::size_t CALL_SIZE_TRACKERS = 1024;

std::map<uint32_t, uint64_t> offset_reuse_freq;
std::map<uint64_t, std::set<uint8_t>> offset_sizes_by_target;
std::set<uint64_t> branch_ip;
std::array<uint64_t, 64> dynamic_bit_counts;
std::array<uint64_t, 64> static_bit_counts;

enum BTB_ReplacementStrategy { LRU, REF0, REF };

struct BTBEntry {
  uint64_t ip_tag = 0;
  uint64_t target;
  branch_info type = branch_info::ALWAYS_TAKEN;
  uint16_t offset_tag = 0;
  uint8_t target_size = 64; // TODO: Only update for which we have sizes
  uint64_t offset_mask = -1;

  auto index() const { return (ip_tag >> 2) & _INDEX_MASK; }
  auto tag() const
  {
    auto tag = ip_tag >> 2 >> _BTB_SET_BITS;
    if (!_BTB_CLIPPED_TAG) {
      return tag;
    }
    tag &= _TAG_MASK;
    if (_BTB_TAG_REGIONS) {
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
    tag &= _TAG_MASK;
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
  std::fill(std::begin(::CALL_SIZE[this]), std::end(::CALL_SIZE[this]), 4);
  ::CONDITIONAL_HISTORY[this] = 0;
  _BTB_SET_BITS = champsim::lg2(BTB_SETS);
  if (this->BTB_CLIPPED_TAG) {
    _BTB_CLIPPED_TAG = 1;
    _BTB_TAG_SIZE = this->BTB_TAG_SIZE;
    _BTB_TAG_REGIONS = this->BTB_TAG_REGIONS;
    _BTB_TAG_REGION_SIZE = this->BTB_TAG_REGION_SIZE;
    _REGION_MASK = pow2(_BTB_TAG_REGION_SIZE) - 1;
    _BTB_REGION_BITS = champsim::lg2(_BTB_TAG_REGIONS);
  } else {
    _BTB_TAG_SIZE = 62 - _BTB_SET_BITS;
  }
  _INDEX_MASK = BTB_SETS - 1;
  _TAG_MASK = pow2(_BTB_TAG_SIZE) - 1;
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
  std::optional<::BTBEntry> btb_entry;
  if (_BTB_TAG_REGIONS) {
    auto region_idx_ = ::REGION_BTB.at(this).check_hit_idx({ip});
    if (!region_idx_.has_value()) {
      btb_entry = ::BTB.at(this).check_hit({ip, 0, ::branch_info::ALWAYS_TAKEN, 0}, true);
    } else {
      // use BTB for all other branches + direct calls
      btb_entry = ::BTB.at(this).check_hit({ip, 0, ::branch_info::ALWAYS_TAKEN, region_idx_.value()});
      if (not btb_entry.has_value()) {
        btb_entry = ::BTB.at(this).check_hit({ip, 0, ::branch_info::ALWAYS_TAKEN, 0}, true);
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
  if (_BTB_TAG_REGIONS) {
    region_idx = ::REGION_BTB.at(this).check_hit_idx({ip});
    if (!region_idx.has_value()) {
      opt_entry = ::BTB.at(this).check_hit({ip, branch_target, type, 0}, true);
      if (!opt_entry.has_value() || (opt_entry.has_value() && opt_entry.value().get_prediction() != branch_target)) {
        ::REGION_BTB.at(this).fill(::region_btb_entry_t{ip});
        region_idx = ::REGION_BTB.at(this).check_hit_idx({ip});
      }
    }
  }

  opt_entry = std::nullopt;
  if (region_idx.has_value())
    opt_entry = ::BTB.at(this).check_hit({ip, branch_target, type, region_idx.value()});
  if (opt_entry.has_value()) {
    opt_entry->type = type;
    if (branch_target != 0)
      opt_entry->target = branch_target;
  }

  std::optional<::BTBEntry> replaced_entry = std::nullopt;
  if (branch_target != 0 && (region_idx.has_value() || !_BTB_TAG_REGIONS)) {
    replaced_entry = ::BTB.at(this).fill(opt_entry.value_or(::BTBEntry{ip, branch_target, type, region_idx.value_or(0)}), num_bits);
    uint64_t new_region = (ip >> 2 >> _BTB_SET_BITS >> _BTB_TAG_SIZE) & _REGION_MASK;
    region_tag_entry_count[new_region]++;
  }

  if (replaced_entry.has_value()) {
    sim_stats.btb_updates++;
    auto new_tag = std::bitset<64>(::BTBEntry{ip, branch_target, type, region_idx.value()}.tag());
    for (size_t idx = 0; idx < 64; idx++) {
      sim_stats.btb_tag_entropy[idx] += new_tag[idx];
    }
    uint64_t old_region = (replaced_entry.value().ip_tag >> 2 >> _BTB_SET_BITS >> _BTB_TAG_SIZE) & _REGION_MASK;
    assert(region_tag_entry_count[old_region] > 0);
    region_tag_entry_count[old_region]--;
    if (region_tag_entry_count[old_region] == 0) {
      region_tag_entry_count.erase(region_tag_entry_count.find(old_region));
    }
    if (!warmup) {
      if (sim_stats.max_regions < region_tag_entry_count.size()) {
        sim_stats.max_regions = region_tag_entry_count.size();
      }
      uint64_t min2ref = std::count_if(region_tag_entry_count.begin(), region_tag_entry_count.end(), [](auto entry) { return entry.second > 3; });
      if (sim_stats.min_regions > min2ref) {
        sim_stats.min_regions = min2ref;
      }
    }
  }
}

/*
void init_btb(int32_t Sets, int32_t Assoc)
{
  numSets = Sets;
  assoc = Assoc;
  // aBTBSize must be a power of 2
  assert(((Sets - 1) & (Sets)) == 0);
  theBTB.resize(Sets);
  indexMask = Sets - 1;
  numIndexBits = (uint32_t)log2((double)Sets);
}

BTBEntry* get_BTBentry(uint64_t ip)
{
  int idx = index(ip);
  uint64_t tag = get_tag(ip);
  uint32_t i = 0;

  while (i < theBTB[idx].size()) {
    // std::cout << "BTB SIZE: " << theBTB[idx].size() << endl;
    // TODO: Catch aliasing here
    if (theBTB[idx][i].tag == tag) {
      return &(theBTB[idx][i]);
    }
    i++;
  }
  return NULL;
}

BTBEntry* get_lru_BTBEntry(uint64_t ip)
{
  int idx = index(ip);

  if (theBTB[idx].size() < assoc) {
    return NULL;
  }

  return &theBTB[idx].front();
}

BTBEntry* update_BTB(uint64_t ip, uint8_t b_type, uint64_t target, uint8_t taken, uint64_t lru_counter, bool wrong_predict = false)
{
  int idx = index(ip);
  uint64_t tag = get_tag(ip);
  int way = -1;
  uint64_t full_tag = get_tag(ip, true);
  for (uint32_t i = 0; i < theBTB[idx].size(); i++) {
    // TODO: Catch aliasing here
    if (theBTB[idx][i].tag == tag) {
      way = i;
      if (!wrong_predict) {
        full_tag = theBTB[idx][i].full_tag;
      }
      break;
    }
  }

  if (way == -1) {
    if ((target != 0) && taken) {
      BTBEntry entry;
      entry.tag = tag;
      entry.full_tag = full_tag;
      entry.branch_type = b_type;
      entry.target_ip = target;
      entry.lru = lru_counter;

      if (theBTB[idx].size() >= assoc) {
        theBTB[idx].erase(theBTB[idx].begin());
      }
      theBTB[idx].push_back(entry);
    } else {
      assert(0);
    }
  } else {
    BTBEntry entry = theBTB[idx][way];
    entry.branch_type = b_type;
    if (target != 0) {
      entry.target_ip = target;
    }
    entry.lru = lru_counter;
    entry.full_tag = full_tag; // check that this is only updated if it is a wrong predict

    // Update LRU
    theBTB[idx].erase(theBTB[idx].begin() + way);
    theBTB[idx].push_back(entry);
  }

  return &(theBTB[idx].back());
}

uint64_t get_lru_value(uint64_t ip)
{
  int idx = index(ip);
  uint64_t lru_value;
  if (theBTB[idx].size() < assoc) { // All ways are not yet allocated
    lru_value = 0;
  } else {
    lru_value = theBTB[idx][0].lru;
    for (uint32_t i = 1; i < theBTB[idx].size(); i++) { // We should never enter here because head should be LRU
      if (theBTB[idx][i].lru < lru_value) {
        assert(0);
      }
    }
  }

  return lru_value;
}

struct offsetBTB {
  std::vector<std::vector<offset_BTBEntry>> theOffsetBTB;
  uint32_t numSets;
  uint32_t assoc;
  uint64_t indexMask;
  uint32_t numIndexBits;
  BTB_ReplacementStrategy strategy;

  offsetBTB() {}

  offsetBTB(int32_t Sets, int32_t Assoc) : numSets(Sets), assoc(Assoc)
  {
    // aBTBSize must be a power of 2
    assert(((Sets - 1) & (Sets)) == 0);
    theOffsetBTB.resize(Sets);
    indexMask = Sets - 1;
    numIndexBits = (uint32_t)log2((double)Sets);
    strategy = REF;
  }

  void init_offsetbtb(int32_t Sets, int32_t Assoc)
  {
    numSets = Sets;
    assoc = Assoc;
    // aBTBSize must be a power of 2
    assert(((Sets - 1) & (Sets)) == 0);
    theOffsetBTB.resize(Sets);
    indexMask = Sets - 1;
    numIndexBits = (uint32_t)log2((double)Sets);
    strategy = REF;
  }

  int get_replacement_candidate(int set)
  {
    size_t way = 0;
    size_t j = 0;
    uint64_t lru_count = theOffsetBTB[set][0].lru;
    uint64_t ref_count = theOffsetBTB[set][0].ref_count;
    switch (strategy) {
    case LRU:
      for (uint32_t i = 1; i < theOffsetBTB[set].size(); i++) {
        if (theOffsetBTB[set][i].lru < lru_count) {
          lru_count = theOffsetBTB[set][i].lru;
          way = i;
        }
      }
      break;
    case REF0:
      while (ref_count != 0 and j < theOffsetBTB[set].size()) {
        if (ref_count > theOffsetBTB[set][j].ref_count) {
          ref_count = theOffsetBTB[set][j].ref_count;
          way = j;
        }
        j++;
      }
      // TODO: make this configurable: right now we only filter 0 entries
      if (ref_count != 0) {
        strategy = LRU;
        way = get_replacement_candidate(set);
        strategy = REF0;
      }
      break;
    case REF:
      while (ref_count != 0 and j < theOffsetBTB[set].size()) {
        if (ref_count > theOffsetBTB[set][j].ref_count) {
          ref_count = theOffsetBTB[set][j].ref_count;
          way = j;
        }
        j++;
      }
      break;
    default:
      assert(0);
    }
    return way;
  }

  int32_t index(uint64_t target) { return knob_intel ? (target & indexMask) : ((target >> 2) & indexMask); }

  void inc_ref_count(int set, int way) { theOffsetBTB[set][way].ref_count++; }
  void dec_ref_count(int set, int way)
  {
    if (((size_t)set) >= theOffsetBTB.size() or ((size_t)way) >= theOffsetBTB[set].size()) {
      std::cerr << "OUTSIDE ACCEPTABLE RANGE FOR OFFSET BTB" << std::endl;
      return;
    }
    if (theOffsetBTB[set][way].ref_count > 0)
      theOffsetBTB[set][way].ref_count--;
  }

  std::pair<uint32_t, uint32_t> update_OffsetBTB(uint64_t target_offset, uint64_t lru_counter, bool is_new_entry)
  {
    int idx = index(target_offset);
    int way = -1;
    for (uint32_t i = 0; i < theOffsetBTB[idx].size(); i++) {
      if (theOffsetBTB[idx][i].target_offset == target_offset) {
        way = i;
        inc_ref_count(idx, way);
        break;
      }
    }

    // cout << "idx " << idx << " way " << way << " offset " << target_offset << endl;
    if (is_new_entry) {
      if (way == -1) {
        offset_not_found_on_BTBmiss++;
      } else {
        offset_found_on_BTBmiss++;
      }
    } else {
      if (way == -1) {
        offset_not_found_on_BTBhit++;
      }
    }

    if (way == -1) {
      if (1) { //(target_offset != 0) &&
        offset_BTBEntry entry;
        entry.target_offset = target_offset;
        entry.lru = lru_counter;
        entry.ref_count = 1;

        // Find and replace LRU entry
        if (theOffsetBTB[idx].size() < assoc) {
          way = theOffsetBTB[idx].size();
          theOffsetBTB[idx].push_back(entry);
        } else {
          way = get_replacement_candidate(idx);

          uint32_t ref_count = theOffsetBTB[idx][way].ref_count;
          map<uint32_t, uint64_t>::iterator it;
          it = offset_reuse_freq.find(ref_count);
          if (it != offset_reuse_freq.end()) {
            it->second++;
          } else {
            offset_reuse_freq[ref_count] = 1;
          }

          theOffsetBTB[idx][way] = entry; // TODO: add refcount of LRU element? as we have pointers to the lru elem
        }
        assert(theOffsetBTB[idx].size() <= assoc);
      }
    } else {
      // Update LRU counter
      theOffsetBTB[idx][way].lru = lru_counter;
    }

    // cout << "idx " << idx << " way " << way << endl;
    return std::make_pair(idx, way);
  }

  uint64_t get_offset_from_offsetBTB(uint32_t idx, uint32_t way)
  {
    assert(idx < numSets);
    assert(way < assoc);
    return theOffsetBTB[idx][way].target_offset;
  }
  uint64_t get_refcount_from_offsetBTB(uint32_t idx, uint32_t way)
  {
    assert(idx < numSets);
    assert(way < assoc);
    return theOffsetBTB[idx][way].ref_count;
  }
};

/*BTB BTB_4D(1024, 8);                                   //Storage: (tag:16-bit, branch-type: 2-bit, target-offset: 10-bit) 28*1024*8 = 28KB
BTB BTB_6D(1024, 8);                                   //Storage: (tag:16-bit, branch-type: 2-bit, target-offset: 15-bit) 33*1024*7 = 28.875KB
BTB BTB_8D(1024, 8);                                   //Storage: (tag:16-bit, branch-type: 2-bit, target-offset: 25-bit) 43*1024*8 = 43KB
BTB BTB_12D(512, 8);                                    //Storage: (tag:16-bit, branch-type: 2-bit,   full-target: 64-bit) 82*256*4  = 10.25KB
BTB BTB_18D(512, 8);
BTB BTB_25D(256, 8);
BTB BTB_46D(128, 8);
BTB BTB_Ret(1024, 8);*/
/*
// Way sizes in bits
uint8_t* btb_partition_sizes;

uint8_t* offsetbtb_partition_sizes;

int NUM_BTB_PARTITIONS = -1;
int NUM_NON_INDIRECT_PARTITIONS = -1;
int NUM_OFFSET_BTB_PARTITIONS = -1;
int NUM_SETS = -1;
int LAST_BTB_PARTITION_ID = -1;
BTB** btb_partition;

offsetBTB** offsetBTB_partition;

uint64_t basic_btb_lru_counter[NUM_CPUS];

uint64_t basic_btb_indirect[NUM_CPUS][BASIC_BTB_INDIRECT_SIZE];
uint64_t basic_btb_conditional_history[NUM_CPUS];

uint64_t basic_btb_ras[NUM_CPUS][BASIC_BTB_RAS_SIZE];
int basic_btb_ras_index[NUM_CPUS];
/*
 * The following two variables are used to automatically identify the
 * size of call instructions, in bytes, which tells us the appropriate
 * target for a call's corresponding return.
 * They exist because ChampSim does not model a specific ISA, and
 * different ISAs could use different sizes for call instructions,
 * and even within the same ISA, calls can have different sizes.
 */
/*
uint64_t basic_btb_call_instr_sizes[NUM_CPUS][BASIC_BTB_CALL_INSTR_SIZE_TRACKERS];

uint64_t basic_btb_abs_addr_dist(uint64_t addr1, uint64_t addr2)
{
  if (addr1 > addr2) {
    return addr1 - addr2;
  }

  return addr2 - addr1;
}

void push_basic_btb_ras(uint8_t cpu, uint64_t ip)
{
  basic_btb_ras_index[cpu]++;
  if (basic_btb_ras_index[cpu] == BASIC_BTB_RAS_SIZE) {
    basic_btb_ras_index[cpu] = 0;
  }

  basic_btb_ras[cpu][basic_btb_ras_index[cpu]] = ip;
}

uint64_t peek_basic_btb_ras(uint8_t cpu) { return basic_btb_ras[cpu][basic_btb_ras_index[cpu]]; }

uint64_t pop_basic_btb_ras(uint8_t cpu)
{
  uint64_t target = basic_btb_ras[cpu][basic_btb_ras_index[cpu]];
  basic_btb_ras[cpu][basic_btb_ras_index[cpu]] = 0;

  basic_btb_ras_index[cpu]--;
  if (basic_btb_ras_index[cpu] == -1) {
    basic_btb_ras_index[cpu] += BASIC_BTB_RAS_SIZE;
  }

  return target;
}

uint64_t basic_btb_call_size_tracker_hash(uint64_t ip) { return (ip & (BASIC_BTB_CALL_INSTR_SIZE_TRACKERS - 1)); }

uint64_t basic_btb_get_call_size(uint8_t cpu, uint64_t ip)
{
  uint64_t size = basic_btb_call_instr_sizes[cpu][basic_btb_call_size_tracker_hash(ip)];

  return size;
}

// TODO: CONVERT TO FLEXIBLE SIZE SOLUTION TO SUPPORT BTBX
int convert_offsetBits_to_btb_partitionID(int num_bits, bool ignore_offset_count = false)
{
  int j = 0;
  for (int i = 0; i < NUM_BTB_PARTITIONS; i++) {
    int way_size = *(btb_partition_sizes + i);
    if (ignore_offset_count or i <= NUM_NON_INDIRECT_PARTITIONS or i == LAST_BTB_PARTITION_ID)
      j = i;
    if (num_bits <= way_size) {
      break;
    }
  }
  return j;
}

// TODO: Fix make fully configurable
int convert_offsetBits_to_offsetbtb_partitionID(int num_bits)
{
  if (num_bits <= offsetbtb_partition_sizes[0] /* 9 *///)
//{
//  return 0;
//}
// else if (num_bits <= offsetbtb_partition_sizes[1] /* 11 */)
//{
//  return 1;
//}
// else if (num_bits <= offsetbtb_partition_sizes[2] /* 19 */)
//{
//  return 2;
//}
// else if (num_bits <= offsetbtb_partition_sizes[3] /* 25 */)
//{
//  return 3;
//}
// else
//{
//  cout << "Num bits " << dec << num_bits << endl;
//  assert(0);
//}
//}
//
// int get_lru_partition(int start_partitionID, uint64_t ip)
//{
//  int lru_partition = start_partitionID;
//  uint64_t lru_value = btb_partition[start_partitionID]->get_lru_value(ip);
//  for (int i = start_partitionID + 1; i < NUM_BTB_PARTITIONS; i++) {
//    uint64_t partition_lru_value = btb_partition[i]->get_lru_value(ip);
//    if (partition_lru_value < lru_value) {
//      lru_partition = i;
//      lru_value = partition_lru_value;
//    }
//  }
//  return lru_partition;
//}
//
// void O3_CPU::initialize_btb()
//{
//  std::cout << "Basic BTB sets: " << BTB_SETS << " ways: " << BTB_WAYS << " indirect buffer size: " << BASIC_BTB_INDIRECT_SIZE
//            << " RAS size: " << BASIC_BTB_RAS_SIZE << std::endl;
//  btb_partition_sizes = btb_sizes;
//  NUM_SETS = BTB_SETS;
//  NUM_BTB_PARTITIONS = BTB_WAYS + 1; // +1 for mini BTB
//  NUM_NON_INDIRECT_PARTITIONS = BTB_NON_INDIRECT;
//  offsetbtb_partition_sizes = btb_partition_sizes + NUM_NON_INDIRECT_PARTITIONS;
//  NUM_OFFSET_BTB_PARTITIONS = (NUM_BTB_PARTITIONS - NUM_NON_INDIRECT_PARTITIONS - 1);
//  LAST_BTB_PARTITION_ID = BTB_WAYS;
//  btb_partition = (BTB**)malloc(NUM_BTB_PARTITIONS * sizeof(BTB*));
//  offsetBTB_partition = (offsetBTB**)malloc(NUM_OFFSET_BTB_PARTITIONS * sizeof(offsetBTB**));
//
//  for (uint32_t i = 0; i < BASIC_BTB_RAS_SIZE; i++) {
//    basic_btb_ras[cpu][i] = 0;
//  }
//  basic_btb_ras_index[cpu] = 0;
//  for (uint32_t i = 0; i < BASIC_BTB_CALL_INSTR_SIZE_TRACKERS; i++) {
//    basic_btb_call_instr_sizes[cpu][i] = 4;
//  }
//
//  basic_btb_lru_counter[cpu] = 0;
//
//  offset_reuse_freq.clear();
//  offset_found_on_BTBmiss = 0;
//  offset_not_found_on_BTBmiss = 0;
//  offset_not_found_on_BTBhit = 0;
//
//  uint64_t num_region_bits = (BTB_REGIONS == 0) ? 0 : (uint64_t)std::log2(BTB_REGIONS);
//  region_mask = ((uint64_t)-1) << (64 - num_region_bits);
//  for (uint32_t i = 0; i < BTB_WAYS; i++) {
//    btb_partition[i] = new BTB(BTB_SETS, 1);
//    btb_partition[i]->full_tag = full_tag;
//    btb_partition[i]->clipped_tag = clipped_tag;
//    btb_partition[i]->num_low_bits = clipped_tag_size;
//    btb_partition[i]->numRegions = BTB_REGIONS;
//  }
//  uint32_t small_btb_sets = BTB_SETS / 8;
//  if (small_btb_sets == 0)
//    small_btb_sets = 1;
//  btb_partition[BTB_WAYS] = new BTB(small_btb_sets, 1);
//
//  for (int i = 0; i < NUM_OFFSET_BTB_PARTITIONS; i++)
//    offsetBTB_partition[i] = new offsetBTB(1, offset_btb_sets[i]); // TODO: make sets/ways configurable
//}
//
// BTB_outcome O3_CPU::btb_prediction(uint64_t ip, uint8_t branch_type)
//{
//  BTBEntry* btb_entry = NULL;
//  int partitionID = -1;
//
//  bool is_static = branch_ip.insert(ip).second;
//  if (is_static) {
//    static_branch_count += 1;
//  }
//  dynamic_branch_count += 1;
//
//  for (int j = 0; j < 64; j++) {
//    if ((ip >> j) & 0x1) {
//      static_bit_counts[j] += (is_static) ? 1 : 0;
//      dynamic_bit_counts[j] += 1;
//    }
//  }
//
//  total_lookups += 1;
//  bool found = false;
//  for (int j = 0; j < NUM_BTB_PARTITIONS && !found; j++) {
//    int idx = btb_partition[j]->index(ip);
//    uint64_t tag = btb_partition[j]->get_tag(ip);
//    uint32_t i = 0;
//
//    while (i < btb_partition[j]->theBTB[idx].size()) {
//      // std::cout << "BTB SIZE: " << theBTB[idx].size() << endl;
//      // TODO: Catch aliasing here
//      if (btb_partition[j]->theBTB[idx][i].tag == tag) {
//        uint64_t lookup_full_tag = btb_partition[j]->get_tag(ip, true);
//        if (btb_partition[j]->theBTB[idx][i].full_tag != lookup_full_tag) {
//          aliasing_overall += 1;
//          if ((lookup_full_tag & region_mask) == (btb_partition[j]->theBTB[idx][i].full_tag & region_mask)) {
//            aliasing_same_region += 1;
//          } else {
//            aliasing_different_region += 1;
//          }
//        }
//        found = true;
//        break;
//      }
//      i++;
//    }
//  }
//
//  for (int i = 0; i < NUM_BTB_PARTITIONS; i++) {
//    btb_entry = btb_partition[i]->get_BTBentry(ip);
//    if (btb_entry) {
//      partitionID = i;
//      break;
//    }
//  }
//
//  if (btb_entry == NULL) {
//    // no prediction for this IP
//    if (branch_type == BRANCH_DIRECT_CALL || branch_type == BRANCH_INDIRECT_CALL) {
//      push_basic_btb_ras(cpu, ip);
//    }
//    BTB_outcome outcome = {0, BRANCH_CONDITIONAL, 2 /*To indicate that it was a BTB miss*/};
//    return outcome;
//  }
//
//  branch_type = NOT_BRANCH;
//  branch_type = btb_entry->branch_type;
//
//  // uint8_t always_taken = false;
//  // if (branch_type != BRANCH_CONDITIONAL) {
//  // always_taken = true;
//  //}
//
//  if ((branch_type == BRANCH_DIRECT_CALL) || (branch_type == BRANCH_INDIRECT_CALL)) {
//    // add something to the RAS
//    push_basic_btb_ras(cpu, ip);
//  }
//
//  if (branch_type == BRANCH_RETURN) {
//    // peek at the top of the RAS
//    uint64_t target = peek_basic_btb_ras(cpu);
//    // and adjust for the size of the call instr
//    target += basic_btb_get_call_size(cpu, target);
//
//    BTB_outcome outcome = {target, BRANCH_RETURN, 0};
//    return outcome;
//  } /*else if ((branch_type == BRANCH_INDIRECT) ||
//             (branch_type == BRANCH_INDIRECT_CALL)) {
//    return std::make_pair(basic_btb_indirect[cpu][basic_btb_indirect_hash(cpu, ip)], always_taken);
//  } */
//  else {
//    // use BTB for all other branches + direct calls
//
//    uint64_t branch_target = btb_entry->target_ip;
//    assert(partitionID != -1 && partitionID < NUM_BTB_PARTITIONS);
//    if (partitionID >= NUM_NON_INDIRECT_PARTITIONS && partitionID != LAST_BTB_PARTITION_ID) {
//      // cout << "Going to read offset. Partition " << std::hex << partitionID << " ip " << ip << " set " << btb_entry->offsetBTB_set << " way " <<
//      // btb_entry->offsetBTB_way << endl;
//      uint64_t target_offset =
//          offsetBTB_partition[btb_entry->offsetBTB_partitionID]->get_offset_from_offsetBTB(btb_entry->offsetBTB_set, btb_entry->offsetBTB_way);
//      int offset_size = offsetbtb_partition_sizes[btb_entry->offsetBTB_partitionID];
//      if (knob_intel) {
//        branch_target = ((ip >> offset_size) << offset_size) | target_offset;
//      } else {
//        uint64_t temp = ((ip >> offset_size) >> 2) << offset_size;
//        branch_target = (temp | target_offset) << 2;
//      }
//      /*if (btb_entry->target_ip != branch_target) {
//        cout << "Partition ID " <<  partitionID << " correct target " << std::hex << btb_entry->target_ip << " computed target " << branch_target << " offset
//        "
//      << target_offset << endl; assert(0);
//      }*/
//    }
//
//    BTB_outcome outcome = {branch_target, branch_type, 0};
//    return outcome;
//  }
//
//  assert(0);
//  // return std::make_pair(0, always_taken);
//}
//
// void assert_refcounts()
//{
//  uint64_t num_idx_entries = 0;
//  uint64_t stored_ref_count = 0;
//  for (int partition = 0; partition < NUM_OFFSET_BTB_PARTITIONS; partition++) {
//    for (auto const& sets : btb_partition[partition + NUM_NON_INDIRECT_PARTITIONS]->theBTB) {
//      for (auto const& entry : sets) {
//        if (entry.tag != 0 and entry.branch_type != BRANCH_RETURN)
//          num_idx_entries += 1;
//      }
//    }
//    for (auto const& sets : offsetBTB_partition[partition]->theOffsetBTB) {
//      for (auto const& entry : sets) {
//        stored_ref_count += entry.ref_count;
//      }
//    }
//  }
//  assert(stored_ref_count == num_idx_entries);
//}
//
// void update_offsetbtb(uint64_t branch_target, int num_offset_bits, BTBEntry* btb_entry, uint64_t lru_counter, bool is_new_entry)
//{
//  int offsetBTB_partitionID = convert_offsetBits_to_offsetbtb_partitionID(num_offset_bits);
//  assert(offsetBTB_partitionID < NUM_OFFSET_BTB_PARTITIONS);
//  uint64_t target_offset_mask = ((uint64_t)1 << offsetbtb_partition_sizes[offsetBTB_partitionID]) - 1;
//  uint64_t target_offset = knob_intel ? (branch_target & target_offset_mask) : (branch_target >> 2) & target_offset_mask;
//  if (not is_new_entry)
//    offsetBTB_partition[btb_entry->offsetBTB_partitionID]->dec_ref_count(btb_entry->offsetBTB_set, btb_entry->offsetBTB_way);
//  std::pair<uint32_t, uint32_t> offsetBTB_index = offsetBTB_partition[offsetBTB_partitionID]->update_OffsetBTB(target_offset, lru_counter, is_new_entry);
//  assert_refcounts();
//
//  btb_entry->offsetBTB_partitionID = offsetBTB_partitionID;
//  btb_entry->offsetBTB_set = offsetBTB_index.first;
//  btb_entry->offsetBTB_way = offsetBTB_index.second;
//}
//
// bool is_offset_ref(BTBEntry* entry, int partitionID)
//{
//  return NUM_NON_INDIRECT_PARTITIONS <= partitionID and partitionID != LAST_BTB_PARTITION_ID and entry->branch_type != BRANCH_RETURN;
//}
//
// uint64_t get_target_from_offset_btb_entry(BTBEntry* entry, uint64_t ip, int partitionID)
//{
//  if (not(is_offset_ref(entry, partitionID)))
//    return entry->target_ip;
//  uint64_t offset = offsetBTB_partition[entry->offsetBTB_partitionID]->get_offset_from_offsetBTB(entry->offsetBTB_set, entry->offsetBTB_way);
//  offset <<= 2;
//  int num_bits = offsetbtb_partition_sizes[entry->offsetBTB_partitionID];
//  uint64_t mask = pow(2, num_bits + 2) - 1;
//  auto target = (ip & ~mask) | (offset & mask);
//  if (offsetBTB_partition[entry->offsetBTB_partitionID]->get_refcount_from_offsetBTB(entry->offsetBTB_set, entry->offsetBTB_way) > LOG_COMMONALITY_CUTOFF) {
//    cout << "common offset: " << endl;
//    cout << "\tip:          0x" << hex << ip << dec << endl;
//    cout << "\toffset:      0x" << hex << offset << dec << endl;
//    cout << "\tprediction:  0x" << hex << target << dec << endl;
//  }
//  return target;
//}
//
// void print_map(std::map<uint32_t, uint16_t>& pm)
//{
//  for (auto const& [key, val] : pm) {
//    std::cout << key << ": " << val << std::endl;
//  }
//}
//
// std::map<uint8_t, uint32_t> kernel_enter_branch_types;
// std::map<uint8_t, uint32_t> kernel_exit_branch_types;
// std::map<uint8_t, uint32_t> stack_enter_branch_types;
// std::map<uint8_t, uint32_t> stack_exit_branch_types;
//
// typedef struct _offset {
//  bool operator<(const struct _offset& off) const
//  {
//    if (ip == off.ip) {
//      return false; // TODO: Think about that some more...
//    }
//    if (num_bits == off.num_bits) {
//      return offset < off.offset;
//    } else {
//      return num_bits < off.num_bits;
//    }
//  }
//  uint8_t num_bits;
//  uint64_t offset;
//  uint64_t ip;
//} Offset;
// std::map<uint64_t, std::set<Offset>> offsets_per_cacheline;
//
//// TODO: are never taken branches inserted?
// uint64_t last_stats_cycle = 0;
// void O3_CPU::update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
//{
//   // updates for indirect branches
//   /*if ((branch_type == BRANCH_INDIRECT) ||
//       (branch_type == BRANCH_INDIRECT_CALL)) {
//     basic_btb_indirect[cpu][basic_btb_indirect_hash(cpu, ip)] = branch_target;
//   }
//   if (branch_type == BRANCH_CONDITIONAL) {
//     basic_btb_conditional_history[cpu] <<= 1;
//     if (taken) {
//       basic_btb_conditional_history[cpu] |= 1;
//     }
//   }*/
//
//   if (99998 <= current_cycle - last_stats_cycle) {
//     // std::cout << "Getting copy statistics - cycle: " << current_cycle << std::endl;
//     last_stats_cycle = current_cycle;
//     for (int i = 0; i < NUM_OFFSET_BTB_PARTITIONS; ++i) {
//       std::map<uint32_t, uint16_t> reuse_frequency;
//       std::map<uint64_t, uint64_t> refcounts_by_offset;
//       auto offsetBTB = offsetBTB_partition[i];
//       for (auto& set : offsetBTB->theOffsetBTB)
//         for (auto& entry : set) {
//           reuse_frequency[entry.ref_count] += 1;
//           refcounts_by_offset[entry.target_offset] = entry.ref_count;
//           if (entry.ref_count > LOG_COMMONALITY_CUTOFF) {
//             std::cout << "Large shared refcount: " << endl;
//             cout << "\tTARGET OFFSET: " << std::bitset<64>(entry.target_offset) << endl;
//             cout << "\tREF COUNT:     " << entry.ref_count << endl;
//             cout << "\tPARTITION:     " << i << endl;
//           }
//         }
//       // sharing_in_btb_by_partition[i].push_back(reuse_frequency);
//       // offset_refcounts_by_partition[i].push_back(refcounts_by_offset);
//     }
//   }
//
//   if (branch_type == BRANCH_RETURN) {
//     // recalibrate call-return offset
//     // if our return prediction got us into the right ball park, but not the
//     // exactly correct byte target, then adjust our call instr size tracker
//     uint64_t call_ip = pop_basic_btb_ras(cpu);
//     uint64_t estimated_call_instr_size = basic_btb_abs_addr_dist(call_ip, branch_target);
//     if (estimated_call_instr_size <= 10) {
//       basic_btb_call_instr_sizes[cpu][basic_btb_call_size_tracker_hash(call_ip)] = estimated_call_instr_size;
//     }
//   }
//
//   if (taken == false)
//     return;
//
//   // Exit kernel
//   if (is_kernel(ip) and not is_kernel(branch_target)) {
//     kernel_exit_branch_types[branch_type]++;
//     // cout << "kernel exit" << endl;
//   }
//
//   // Enter kernel
//   if (is_kernel(branch_target) and not is_kernel(ip)) {
//     kernel_enter_branch_types[branch_type]++;
//     // cout << "kernel enter" << endl;
//   }
//
//   // Enter stack
//   if (is_shared_or_vdso(branch_target) and not is_shared_or_vdso(ip)) {
//     stack_enter_branch_types[branch_type]++;
//     // cout << "stack enter" << endl;
//   }
//
//   // Exit stack
//   if (is_shared_or_vdso(ip) and not(is_shared_or_vdso(branch_target) or is_kernel(branch_target))) {
//     stack_exit_branch_types[branch_type]++;
//     // cout << "stack exit" << endl;
//   }
//
//   bool wrong_predicted = true;
//   BTBEntry* btb_entry = NULL;
//   int partitionID = -1;
//   for (int i = 0; i < NUM_BTB_PARTITIONS; i++) {
//     btb_entry = btb_partition[i]->get_BTBentry(ip);
//     if (btb_entry) {
//       partitionID = i;
//       wrong_predicted = false;
//       break;
//     }
//   }
//
//   int num_bits;
//   uint64_t branch_cacheline_tag = ip >> LOG2_BLOCK_SIZE;
//   if (branch_type == BRANCH_RETURN) {
//     num_bits = 0;
//   } else {
//     uint64_t diff_bits = knob_intel ? (branch_target ^ ip) : (branch_target >> 2) ^ (ip >> 2);
//     num_bits = 0;
//     while (diff_bits != 0) {
//       diff_bits = diff_bits >> 1;
//       num_bits++;
//     }
//   }
//   extern uint8_t knob_collect_offsets;
//   if (knob_collect_offsets) {
//     uint64_t offset_mask = ((1ull << num_bits) - 1);
//     uint64_t offset = knob_intel ? branch_target & offset_mask : (branch_target >> 2) & offset_mask;
//     auto inserted = pc_offset_pairs_by_size[num_bits].insert(std::make_pair(ip, offset));
//
//     Offset off{.num_bits = (uint8_t)num_bits, .offset = offset, .ip = ip};
//     offsets_per_cacheline[branch_cacheline_tag].insert(off); // TODO: only max per ip?
//     offset_size_count[num_bits - 1]++;
//     offset_counts_by_size[num_bits - 1][offset] += 1; // static inserts: inserted.second ? 1 : 0
//     inserted = pc_offset_pairs_by_partition[convert_offsetBits_to_btb_partitionID(num_bits, true)].insert(std::make_pair(ip, offset));
//     static_offset_counts_by_partition[convert_offsetBits_to_btb_partitionID(num_bits, true)][offset] += inserted.second ? 1 : 0;
//      static_branch_pc_counts_by_partition[convert_offsetBits_to_btb_partitionID(num_bits, true)][ip] += inserted.second ? 1 : 0;
//      static_target_pc_counts_by_partition[convert_offsetBits_to_btb_partitionID(num_bits, true)][branch_target] += inserted.second ? 1 : 0;
//      dynamic_offset_counts_by_partition[convert_offsetBits_to_btb_partitionID(num_bits, true)][offset] += 1;
//      dynamic_branch_pc_counts_by_partition[convert_offsetBits_to_btb_partitionID(num_bits, true)][ip] += 1;
//      dynamic_target_pc_counts_by_partition[convert_offsetBits_to_btb_partitionID(num_bits, true)][branch_target] += 1;
//      type_counts_by_size[convert_offsetBits_to_btb_partitionID(num_bits, true)][branch_type] += inserted.second ? 1 : 0;
//    }
//
//    assert_refcounts();
//    assert(num_bits >= 0 && num_bits < 66);
//    uint64_t predicted_target = 0;
//    if (btb_entry != NULL)
//      predicted_target = get_target_from_offset_btb_entry(btb_entry, ip, partitionID);
//    if (btb_entry != NULL && predicted_target != branch_target) {
//      if (is_offset_ref(btb_entry, partitionID)) {
//        offsetBTB_partition[btb_entry->offsetBTB_partitionID]->dec_ref_count(btb_entry->offsetBTB_set, btb_entry->offsetBTB_way);
//      }
//      btb_entry->full_tag = 0;
//      btb_entry->tag = 0;
//      btb_entry->lru = 0;
//      btb_entry->target_ip = 0;
//      btb_entry->branch_type = NOT_BRANCH;
//      btb_entry->offsetBTB_partitionID = 0;
//      btb_entry->offsetBTB_set = -1;
//      btb_entry->offsetBTB_way = -1;
//      btb_entry = NULL;
//      wrong_predicted = true;
//    }
//
//    if (btb_entry == NULL) {
//      BTB_writes++;
//
//      int smallest_offset_partition_id = convert_offsetBits_to_btb_partitionID(num_bits);
//
//      int partition = get_lru_partition(smallest_offset_partition_id, ip);
//      assert(partition < NUM_BTB_PARTITIONS);
//
//      if (NUM_NON_INDIRECT_PARTITIONS <= partition and partition != NUM_BTB_PARTITIONS - 1) {
//        BTBEntry* prev_entry = btb_partition[partition]->get_lru_BTBEntry(ip);
//        if (prev_entry and prev_entry->tag and prev_entry->branch_type != BRANCH_RETURN)
//          offsetBTB_partition[prev_entry->offsetBTB_partitionID]->dec_ref_count(prev_entry->offsetBTB_set, prev_entry->offsetBTB_way);
//      }
//
//      // cout << "IP " << ip << " Target " << branch_target << " mask " << target_offset_mask << " offset " << target_offset << " num bits " << num_bits <<
//      // endl;
//
//      BTBEntry* btb_entry = btb_partition[partition]->update_BTB(ip, branch_type, branch_target, taken, basic_btb_lru_counter[cpu], true);
//      if (is_offset_ref(btb_entry, partition)) {
//        offset_sizes_by_target[branch_target].insert(num_bits);
//        update_offsetbtb(branch_target, num_bits, btb_entry, basic_btb_lru_counter[cpu], 1);
//      }
//      assert_refcounts();
//
//      basic_btb_lru_counter[cpu]++;
//
//      // cout << "stored IP " << std::hex << ip << " target " << branch_target << " in partition " << partition << endl;
//
//    } else {
//      // update an existing entry
//      assert(partitionID != -1);
//      BTBEntry* btb_entry = btb_partition[partitionID]->update_BTB(ip, branch_type, branch_target, taken, basic_btb_lru_counter[cpu], wrong_predicted);
//
//      if (partitionID >= NUM_NON_INDIRECT_PARTITIONS && partitionID != LAST_BTB_PARTITION_ID && branch_type != BRANCH_RETURN) {
//        offset_sizes_by_target[branch_target].insert(num_bits);
//        update_offsetbtb(branch_target, num_bits, btb_entry, basic_btb_lru_counter[cpu], 0);
//      }
//      basic_btb_lru_counter[cpu]++;
//    }
//  }
//
//  void O3_CPU::btb_final_stats()
//  {
//    cout << "XXX offset_found_on_BTBmiss " << offset_found_on_BTBmiss << endl;
//    cout << "XXX offset_not_found_on_BTBmiss " << offset_not_found_on_BTBmiss << endl;
//    cout << "XXX offset_not_found_on_BTBhit " << offset_not_found_on_BTBhit << endl;
//    uint64_t total_offsetBTB_evictions = 0;
//    for (map<uint32_t, uint64_t>::iterator it = offset_reuse_freq.begin(); it != offset_reuse_freq.end(); ++it)
//      total_offsetBTB_evictions += it->second;
//
//    std::map<uint32_t, uint64_t> num_cachelines_required_n_bits;
//
//    for (const auto& cacheline : offsets_per_cacheline) {
//      uint16_t offset_total_size = 0;
//      for (const auto& offset : cacheline.second) {
//        offset_total_size += offset.num_bits;
//      }
//      num_cachelines_required_n_bits[offset_total_size]++;
//    }
//
//    std::cout << "XXX num_cachelines_required_n_bits" << std::endl;
//    for (const auto& n_bits_cacheline_count : num_cachelines_required_n_bits) {
//      std::cout << n_bits_cacheline_count.first << ":\t" << n_bits_cacheline_count.second << std::endl;
//    }
//
//    cout << "XXX total_offsetBTB_evictions " << total_offsetBTB_evictions << endl;
//    int i = 0;
//    uint64_t offsetBTB_evictions = 0;
//    for (map<uint32_t, uint64_t>::iterator it = offset_reuse_freq.begin(); it != offset_reuse_freq.end() && i < 10; ++it, i++) {
//      std::cout << "XXX reuse" << it->first << " " << it->second << '\n';
//      offsetBTB_evictions += it->second;
//    }
//    std::cout << "XXX reuseM" << " " << (total_offsetBTB_evictions - offsetBTB_evictions) << '\n';
//
//    cout << "XXX Kernel target histogram: " << endl;
//    uint32_t kernel_targets_total = 0;
//    for (auto const& target_stats : offset_sizes_by_target) {
//      if ((target_stats.first >> 48) != 0xFFFF) {
//        continue;
//      }
//      kernel_targets_total++;
//      cout << "0x" << hex << uppercase << target_stats.first << dec << ":\t";
//      for (auto const& offset_size : target_stats.second) {
//        cout << ((uint32_t)offset_size) << ", ";
//      }
//      cout << endl;
//    }
//    cout << "XXX Kernel offset targets total: " << kernel_targets_total << endl;
//
//    cout << "XXX Big offset counts and presence in smaller offset btbs: " << endl;
//    uint32_t count_big_targets = 0;
//    for (auto const& target_stats : offset_sizes_by_target) {
//      if (*(target_stats.second.rbegin()) < 19) {
//        continue;
//      }
//      count_big_targets++;
//      cout << "0x" << hex << uppercase << target_stats.first << dec << ":\t";
//      for (auto const& offset_size : target_stats.second) {
//        cout << ((uint32_t)offset_size) << ", ";
//      }
//      cout << endl;
//    }
//    cout << "XXX Big offset targets total: " << count_big_targets << endl;
//
//    cout << "XXX TOTAL LOOKUPS: " << total_lookups << endl;
//    cout << "XXX TOTAL ALIASING: " << aliasing_overall << endl;
//    cout << "XXX ALIASING SAME REGION: " << aliasing_same_region << endl;
//    cout << "XXX ALIASING OTHER REGION: " << aliasing_different_region << endl;
//
//    cout << "XXX Total dynamic branch IPs: " << dynamic_branch_count << endl;
//    cout << "XXX Total dynamic 1 bits in branch IPs:" << endl;
//    for (int j = 0; j < 64; j++) {
//      cout << j << ":\t" << (double)dynamic_bit_counts[j] / (double)dynamic_branch_count << endl;
//    }
//
//    cout << "XXX Total static branch IPs: " << static_branch_count << endl;
//    cout << "XXX Total static 1 bits in branch IPs:" << endl;
//    for (int j = 0; j < 64; j++) {
//      cout << j << ":\t" << (double)static_bit_counts[j] / (double)static_branch_count << endl;
//    }
//    cout << "XXX END BTB STATS" << endl;
//  }
//
//  bool O3_CPU::is_not_block_ending(uint64_t ip) { return true; }
//