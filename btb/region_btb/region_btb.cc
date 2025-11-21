
/*
 * This file implements a basic Branch Target Buffer (BTB) structure.
 * It uses a set-associative BTB to predict the targets of non-return branches,
 * and it uses a small Return Address Stack (RAS) to predict the target of
 * returns.
 */

 // TODO: FIX FOR REGIONS

#include <algorithm>
#include <bitset>
#include <deque>
#include <map>

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

std::vector<uint8_t> btb_addressing_hash;
std::size_t _INDEX_MASK = 0;
std::size_t _BTB_SETS = 0;
std::size_t _BTB_WAYS = 0;
std::size_t _TAG_MASK = 0;
std::size_t _FULL_TAG_MASK = 0;
std::size_t _BTB_SET_BITS = 0;

uint64_t _BTB_TAG_REGIONS = 0; // This is the number of regions in the region BTB
uint64_t _BTB_TAG_REGION_WAYS = 0;
uint64_t _BTB_TAG_REGION_SETS = 0;
uint64_t _BTB_TAG_REGION_SET_IDX_BITS = 0;
uint8_t _BTB_TAG_REGION_SIZE = 0; // This is the size of a single region in bits
uint64_t _BTB_REGION_BITS = 0; 
uint8_t _BTB_TAG_SIZE = 0;
std::size_t _REGION_MASK = 0;
bool _PERFECT_MAPPING = false;
bool _BTB_CLIPPED_TAG = false;

uint64_t isa_shiftamount = 2;
uint64_t shuffle_ip_tag(uint64_t ip_tag)
{
  if (btb_addressing_hash.empty()) {
    return ip_tag;
  } else {
    std::bitset<64> ip_tag_b{ip_tag};
    std::bitset<64> ip_b{0};
    int i = 0;
    for (; i < btb_addressing_hash.size(); i++) {
      ip_b[i] = ip_tag_b[btb_addressing_hash.at(i)];
    }
    for (; i < 64; i++) {
      ip_b[i] = ip_tag_b[i];
    }
    return ip_b.to_ullong();
  }
}

constexpr std::size_t BTB_INDIRECT_SIZE = 4096;
constexpr std::size_t RAS_SIZE = 64;
constexpr std::size_t CALL_SIZE_TRACKERS = 1024;
constexpr std::size_t TARGETS_PER_ENTRY = 4;

struct btb_target_t {
  uint64_t target = 0;
  uint8_t offset = 0;
  branch_info type = branch_info::ALWAYS_TAKEN;
};

struct current_pred_t {
  uint64_t predicted_on = 0;
  uint64_t branch_ip = 0;
  uint64_t target = 0;
  branch_info type = branch_info::ALWAYS_TAKEN;
};

struct btb_target_t null_target[4] = {};

struct btb_entry_t {
  uint64_t ip_tag = 0;
  struct btb_target_t targets[4] = {};

  // Set / precise / magic pointers
  std::tuple<uint16_t, uint16_t, uint64_t> region_idx_tag = {0, 0, 0};

  btb_entry_t() {
  };
  btb_entry_t(uint64_t ip, btb_target_t target) {
    ip_tag = ip;
    targets[0] = target;
  };
  btb_entry_t(uint64_t ip, btb_target_t *target) {
    ip_tag = ip;
    for (uint8_t i = 0; i < 4; i++) {
      targets[i] = *(target+i);
    }
  };

  auto index() const {
    auto ip = shuffle_ip_tag(ip_tag);
    auto idx = (ip >> isa_shiftamount) & _INDEX_MASK;
    return idx;
  }
  auto tag() const {
    auto ip = shuffle_ip_tag(ip_tag);
    auto tag = ip >> isa_shiftamount >> _BTB_SET_BITS;
    if (!_BTB_CLIPPED_TAG) {
      return tag;
    }

    tag &= _FULL_TAG_MASK;
    if (ip && _BTB_TAG_REGIONS) {
      // TODO: double check if the shift amount of the BTB TAG size is correct and we are not overriding the actual tag bits
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
  auto partial_tag() const { return (ip_tag >> 2 >> 9); }
};
current_pred_t current_pred;
std::map<O3_CPU*, champsim::msl::lru_table<btb_entry_t>> BTB;
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
  std::fill(std::begin(::INDIRECT_BTB[this]), std::end(::INDIRECT_BTB[this]), 0);
  std::fill(std::begin(::CALL_SIZE[this]), std::end(::CALL_SIZE[this]), 4);
  ::CONDITIONAL_HISTORY[this] = 0;
  ::btb_addressing_hash = btb_index_tag_hash;

  _INDEX_MASK = BTB_SETS - 1;
  _BTB_SETS = BTB_SETS;
  _BTB_WAYS = BTB_WAYS;
  _TAG_MASK = pow2(_BTB_TAG_SIZE) - 1;
  _BTB_SET_BITS = champsim::lg2(BTB_SETS);
  _PERFECT_MAPPING = btb_perfect_mapping;
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
  if (intel) {
    isa_shiftamount = 0;
  }
}

std::tuple<uint64_t, uint64_t, uint8_t> O3_CPU::btb_prediction(uint64_t ip)
{
  if (current_pred.predicted_on < ip && ip < current_pred.branch_ip) {
    return {0, ip, false};
  } else if (current_pred.branch_ip == ip) {
    std::tuple<uint64_t, uint64_t, uint8_t> rv = {current_pred.target, ip, current_pred.type != ::branch_info::CONDITIONAL};
    current_pred = {};
    return rv;
  }
  auto offset = ip & (BTB_REGION_SIZE - 1);
  auto base_address = ip & (-1 ^ (BTB_REGION_SIZE -1));

  // use BTB for all other branches + direct calls
  auto btb_entry = ::BTB.at(this).check_hit({base_address, null_target});

  // no prediction for this IP
  if (!btb_entry.has_value()) {
    current_pred = {};
    return {0, ip, false};
  }
  
  std::optional<btb_target_t> target = std::nullopt;
  for (uint8_t i = 0; i < 4; i++) {
    if (btb_entry->targets[i].offset >= offset) {
      target = btb_entry->targets[i];
      break;
    }
  }
  if (!target.has_value()) {
    current_pred = {};
    return {0, btb_entry->ip_tag, false};
  }

  if (target->type == ::branch_info::RETURN) {
    if (std::empty(::RAS[this])) {
      current_pred = {};
      return {0, btb_entry->ip_tag, true};
    }

    // peek at the top of the RAS and adjust for the size of the call instr
    auto ras_target = ::RAS[this].back();
    auto size = ::CALL_SIZE[this][ras_target % std::size(::CALL_SIZE[this])];
    if (target->offset == offset)
      return {ras_target + size, btb_entry->ip_tag, true};
    else {
      current_pred = {.predicted_on=ip, .branch_ip = target->offset + base_address, .target = ras_target + size, .type = target->type};
      return {0, btb_entry->ip_tag, true};
    }
  }

  // if (btb_entry->type == ::branch_info::INDIRECT) {
  //   auto hash = (ip >> 2) ^ ::CONDITIONAL_HISTORY[this].to_ullong();
  //   return {::INDIRECT_BTB[this][hash % std::size(::INDIRECT_BTB[this])], btb_entry->ip_tag, true};
  // }
  if (target->offset == offset) {
    return {target->target, btb_entry->ip_tag, target->type != ::branch_info::CONDITIONAL};
  } else {
    current_pred = {.predicted_on=ip, .branch_ip = target->offset + base_address, .target = target->target, .type = target->type};
    return {0, btb_entry->ip_tag, true};
  }
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
  }


  // update btb entry
  uint8_t offset = ip & (BTB_REGION_SIZE - 1);
  ip = ip & (-1 ^ (BTB_REGION_SIZE -1));
  auto type = ::branch_info::ALWAYS_TAKEN;
  if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL))
    type = ::branch_info::INDIRECT;
  else if (branch_type == BRANCH_RETURN)
    type = ::branch_info::RETURN;
  else if ((branch_type == BRANCH_CONDITIONAL) || (branch_type == BRANCH_OTHER))
    type = ::branch_info::CONDITIONAL;

  auto opt_entry = ::BTB.at(this).check_hit({ip, null_target});
  if (opt_entry.has_value()) {
    // sort targets by offset, treating empty slots (offset==0 && target==0) as largest so they go to the end
    std::stable_sort(std::begin(opt_entry->targets), std::end(opt_entry->targets),
      [](const btb_target_t &a, const btb_target_t &b) {
        auto va = (a.target == 0) ? static_cast<uint8_t>(0xFF) : a.offset;
        auto vb = (b.target == 0) ? static_cast<uint8_t>(0xFF) : b.offset;
        return va < vb;
      });
    for(uint8_t i = 0; i < 4; i++) {
      if (opt_entry->targets[i].offset >= offset) {
        opt_entry->targets[i].type = type;
        if (branch_target) {
          opt_entry->targets[i].target = branch_target;
        }
        break;
      }
      else if (opt_entry->targets[i].target == 0) {
        opt_entry->targets[i].type = type;
        if (branch_target) {
          opt_entry->targets[i].target = branch_target;
        }
        break;
      }
    }
  }

  btb_target_t target[4] = {*null_target};
  target[0].offset = offset;
  target[0].target = branch_target;
  target[0].type = type;
  if (branch_target != 0) {
    ::BTB.at(this).fill(opt_entry.value_or(::btb_entry_t{ip, *target}));
  }
}


void O3_CPU::btb_end_phase(unsigned finished_cpu)
{
}

void O3_CPU::btb_invalidate_entry(uint64_t ip){}