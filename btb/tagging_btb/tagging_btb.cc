
/*
 * This file implements a basic Branch Target Buffer (BTB) structure.
 * It uses a set-associative BTB to predict the targets of non-return branches,
 * and it uses a small Return Address Stack (RAS) to predict the target of
 * returns.
 */

#include <algorithm>
#include <bitset>
#include <deque>
#include <iostream>
#include <map>

#include "msl/lru_table.h"
#include "ooo_cpu.h"

uint64_t pow2(uint8_t exp)
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

struct btb_entry_t {
  uint64_t ip_tag = 0;
  uint64_t target = 0;
  branch_info type = branch_info::ALWAYS_TAKEN;
  uint16_t offset_tag = 0;

  auto index() const { return (ip_tag >> 2) & _INDEX_MASK; }
  auto tag() const
  {
    auto tag = ip_tag >> 2 >> _BTB_SET_BITS;
    if (!_BTB_CLIPPED_TAG) {
      return tag;
    }
    tag &= _TAG_MASK;
    if (_BTB_TAG_REGIONS) {
      // TODO: check if zeroed out proper bits
      auto masked_bits = tag & (_REGION_MASK << _BTB_TAG_SIZE);
      tag ^= masked_bits;
      tag |= offset_tag << _BTB_TAG_SIZE;
    }
    return tag;
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
};

/*
uint64_t patch_ip(uint64_t ip, uint64_t region_idx)
{
  // TODO: Check shamt
  auto shamt = 2 << _BTB_SET_BITS << _BTB_TAG_SIZE;
  auto masked_bits = ip & (_REGION_MASK << shamt);
  ip ^= masked_bits;
  ip |= region_idx << shamt;
  return ip;
}
*/

std::map<O3_CPU*, champsim::msl::lru_table<btb_entry_t>> BTB;
std::map<O3_CPU*, champsim::msl::lru_table<region_btb_entry_t>> REGION_BTB;
std::map<O3_CPU*, std::array<uint64_t, BTB_INDIRECT_SIZE>> INDIRECT_BTB;
std::map<O3_CPU*, std::bitset<champsim::lg2(BTB_INDIRECT_SIZE)>> CONDITIONAL_HISTORY;
std::map<O3_CPU*, std::deque<uint64_t>> RAS;
/*
 * The following structure identifies the size of call instructions so we can
 * find the target for a call's return, since calls may have different sizes.
 */
std::map<O3_CPU*, std::array<uint64_t, CALL_SIZE_TRACKERS>> CALL_SIZE;
} // namespace

void O3_CPU::initialize_btb()
{
  ::BTB.insert({this, champsim::msl::lru_table<btb_entry_t>{BTB_SETS, BTB_WAYS}});
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
}

std::pair<uint64_t, uint8_t> O3_CPU::btb_prediction(uint64_t ip)
{
  uint8_t region_idx = -1;
  if (_BTB_TAG_REGIONS) {
    auto region_idx_ = ::REGION_BTB.at(this).check_hit_idx({ip});
    if (!region_idx_.has_value()) {
      return {0, false};
    }
    region_idx = region_idx_.value();
  }
  // use BTB for all other branches + direct calls
  auto btb_entry = ::BTB.at(this).check_hit({ip, 0, ::branch_info::ALWAYS_TAKEN, region_idx});

  // no prediction for this IP
  if (!btb_entry.has_value())
    return {0, false};

  if (btb_entry->type == ::branch_info::RETURN) {
    if (std::empty(::RAS[this]))
      return {0, true};

    // peek at the top of the RAS and adjust for the size of the call instr
    auto target = ::RAS[this].back();
    auto size = ::CALL_SIZE[this][target % std::size(::CALL_SIZE[this])];

    return {target + size, true};
  }

  if (btb_entry->type == ::branch_info::INDIRECT) {
    auto hash = (ip >> 2) ^ ::CONDITIONAL_HISTORY[this].to_ullong();
    return {::INDIRECT_BTB[this][hash % std::size(::INDIRECT_BTB[this])], true};
  }

  return {btb_entry->target, btb_entry->type != ::branch_info::CONDITIONAL};
}

void O3_CPU::update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  // add something to the RAS
  if (branch_type == BRANCH_DIRECT_CALL || branch_type == BRANCH_INDIRECT_CALL) {
    RAS[this].push_back(ip);
    if (std::size(RAS[this]) > RAS_SIZE)
      RAS[this].pop_front();
  }

  // updates for indirect branches
  if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL)) {
    auto hash = (ip >> 2) ^ ::CONDITIONAL_HISTORY[this].to_ullong();
    ::INDIRECT_BTB[this][hash % std::size(::INDIRECT_BTB[this])] = branch_target;
  }

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
  }

  // update btb entry
  auto type = ::branch_info::ALWAYS_TAKEN;
  if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL))
    type = ::branch_info::INDIRECT;
  else if (branch_type == BRANCH_RETURN)
    type = ::branch_info::RETURN;
  else if ((branch_type == BRANCH_CONDITIONAL) || (branch_type == BRANCH_OTHER))
    type = ::branch_info::CONDITIONAL;

  uint8_t region_idx = -1;
  if (_BTB_TAG_REGIONS) {
    auto region_idx_ = ::REGION_BTB.at(this).check_hit_idx({ip});
    if (!region_idx_.has_value()) {
      ::REGION_BTB.at(this).fill(::region_btb_entry_t{ip});
      region_idx_ = ::REGION_BTB.at(this).check_hit_idx({ip});
      ::BTB.at(this).invalidate_region({ip, 0, ::branch_info::ALWAYS_TAKEN, region_idx_.value()});
    }
    region_idx = region_idx_.value();
  }

  auto opt_entry = ::BTB.at(this).check_hit({ip, branch_target, type, region_idx});
  if (opt_entry.has_value()) {
    opt_entry->type = type;
    if (branch_target != 0)
      opt_entry->target = branch_target;
  }

  if (branch_target != 0) {
    ::BTB.at(this).fill(opt_entry.value_or(::btb_entry_t{ip, branch_target, type, region_idx}));
  }
}

// TODO: Partial hits
// * Aliasing benficial/hurts
