
/*
 * This file implements a basic Branch Target Buffer (BTB) structure.
 * It uses a set-associative BTB to predict the targets of non-return branches,
 * and it uses a small Return Address Stack (RAS) to predict the target of
 * returns.
 */

#include <algorithm>
#include <bitset>
#include <deque>
#include <map>

#include "msl/lru_table.h"
#include "ooo_cpu.h"

namespace
{
enum class branch_info {
  INDIRECT,
  RETURN,
  ALWAYS_TAKEN,
  CONDITIONAL,
};

constexpr std::size_t BTB_SET = 512;
constexpr std::size_t BTB_WAY = 8;
constexpr std::size_t BTB_INDIRECT_SIZE = 4096;
constexpr std::size_t RAS_SIZE = 64;
constexpr std::size_t CALL_SIZE_TRACKERS = 1024;
std::size_t _FULL_TAG_MASK = 0;

struct btb_entry_t {
  uint64_t ip_tag = 0;
  uint64_t target = 0;
  branch_info type = branch_info::ALWAYS_TAKEN;
  std::tuple<uint16_t, uint16_t, uint64_t> region_idx_tag = {0, 0, 0};
  uint8_t target_size = 64; // TODO: Only update for which we have sizes
  uint64_t offset_mask = -1;
  uint8_t precise_branch_type;
  bool useless = false;
  bool replacement_protected = false;

  auto index() const { return ip_tag >> 2 & 511; }
  auto tag() const
  {
    auto tag = ip_tag >> 2 >> 9;
    tag &= _FULL_TAG_MASK;
    return tag;
  }
  auto partial_tag() const { return (ip_tag >> 2 >> 9); }
};

// 64 sets, 8 ways, full tag
struct l1_btb_entry_t {
  uint64_t ip_tag = 0;
  uint64_t target = 0;
  branch_info type = branch_info::ALWAYS_TAKEN;
  std::tuple<uint16_t, uint16_t, uint64_t> region_idx_tag = {0, 0, 0};
  uint8_t target_size = 64; // TODO: Only update for which we have sizes
  uint64_t offset_mask = -1;
  uint8_t precise_branch_type = NOT_BRANCH;
  bool useless = false;
  bool replacement_protected = false;

  auto index() const { return ip_tag >> 2 & 63; }
  auto tag() const { return (ip_tag >> 2 >> 6); }
  auto partial_tag() const { return (ip_tag >> 2 >> 6); }
};

std::map<O3_CPU*, champsim::msl::lru_table<l1_btb_entry_t>> L1_BTB;
std::map<O3_CPU*, champsim::msl::lru_table<btb_entry_t>> L2_BTB;
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
  ::L2_BTB.insert({this, champsim::msl::lru_table<btb_entry_t>{BTB_SET, BTB_WAY}});
  ::L1_BTB.insert({this, champsim::msl::lru_table<l1_btb_entry_t>{64, 8}});
  std::fill(std::begin(::INDIRECT_BTB[this]), std::end(::INDIRECT_BTB[this]), 0);
  std::fill(std::begin(::CALL_SIZE[this]), std::end(::CALL_SIZE[this]), 4);
  ::CONDITIONAL_HISTORY[this] = 0;
  ::_FULL_TAG_MASK = pow2(this->BTB_TAG_SIZE) - 1;
}

void O3_CPU::btb_begin_wrongpath() {}

void O3_CPU::btb_end_wrongpath() {}

void O3_CPU::btb_end_phase(unsigned finished_cpu) {}

void O3_CPU::btb_invalidate_entry(uint64_t ip) {}

std::tuple<uint64_t, uint64_t, uint8_t, uint8_t> O3_CPU::btb_prediction(uint64_t ip)
{
  // use BTB for all other branches + direct calls
  auto l1_btb_entry = ::L1_BTB.at(this).check_hit({ip, 0, ::branch_info::ALWAYS_TAKEN});

  std::optional<std::tuple<uint64_t, uint64_t, uint8_t, uint8_t>> L1_prediction = std::nullopt;
  if (l1_btb_entry.has_value()) {
    if (l1_btb_entry->type == ::branch_info::RETURN) {
      if (std::empty(RAS.at(this))) {
        L1_prediction = {0, l1_btb_entry->ip_tag, true, BRANCH_RETURN};
      } else {
        // peek at the top of the RAS and adjust for the size of the call instr
        auto target = RAS.at(this).back();

        L1_prediction = {target + 4, l1_btb_entry->ip_tag, true, BRANCH_RETURN}; // assume fixed size for now
      }
    } else {
      L1_prediction = {l1_btb_entry->target, l1_btb_entry->ip_tag, l1_btb_entry->type != ::branch_info::CONDITIONAL, l1_btb_entry->precise_branch_type};
    }
  }
  if (L1_prediction.has_value()) {
    sim_stats.l1_btb_hit += 1;
    is_l1_btb_prediction = true;
    return L1_prediction.value();
  }
  is_l1_btb_prediction = false;

  auto btb_entry = ::L2_BTB.at(this).check_hit({ip, 0, ::branch_info::ALWAYS_TAKEN});

  // no prediction for this IP
  if (!btb_entry.has_value())
    return {0, ip, false, 0};

  if (btb_entry->type == ::branch_info::RETURN) {
    if (std::empty(::RAS[this])) {
      sim_stats.l1_btb_hit += 1;
      is_l1_btb_prediction = true;
      return {0, btb_entry->ip_tag, true, 0};
    }

    // peek at the top of the RAS and adjust for the size of the call instr
    auto target = ::RAS[this].back();
    auto size = ::CALL_SIZE[this][target % std::size(::CALL_SIZE[this])];
    sim_stats.l2_btb_hit += 1;

    return {target + size, btb_entry->ip_tag, true, 0};
  }

  // if (btb_entry->type == ::branch_info::INDIRECT) {
  //   auto hash = (ip >> 2) ^ ::CONDITIONAL_HISTORY[this].to_ullong();
  //   return {::INDIRECT_BTB[this][hash % std::size(::INDIRECT_BTB[this])], btb_entry->ip_tag, true};
  // }
  if (btb_entry->target == 0) {
    sim_stats.l1_btb_hit += 1;
    is_l1_btb_prediction = true;
  } else {
    sim_stats.l2_btb_hit += 1;
  }

  return {btb_entry->target, btb_entry->ip_tag, btb_entry->type != ::branch_info::CONDITIONAL, 0};
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
  auto type = ::branch_info::ALWAYS_TAKEN;
  if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL))
    type = ::branch_info::INDIRECT;
  else if (branch_type == BRANCH_RETURN)
    type = ::branch_info::RETURN;
  else if ((branch_type == BRANCH_CONDITIONAL) || (branch_type == BRANCH_OTHER))
    type = ::branch_info::CONDITIONAL;

  std::optional<::l1_btb_entry_t> l1_evicted = std::nullopt;
  auto L1_opt_entry = ::L1_BTB.at(this).check_hit({ip, branch_target, type});
  if (L1_opt_entry.has_value() && branch_target != 0) {
    L1_opt_entry->type = type;
    L1_opt_entry->target = branch_target;
  }

  if (branch_target != 0) {
    auto fill_entry = ::l1_btb_entry_t{ip, branch_target, type};
    fill_entry.precise_branch_type = branch_type;
    l1_evicted = ::L1_BTB.at(this).fill(L1_opt_entry.value_or(fill_entry), 0);
  }

  if (!l1_evicted.has_value() || l1_evicted->ip_tag == ip) {
    return;
  }

  ip = l1_evicted->ip_tag;
  branch_target = l1_evicted->target;
  taken = true;
  branch_type == l1_evicted->precise_branch_type;
  type = l1_evicted->type;

  auto opt_entry = ::L2_BTB.at(this).check_hit({ip, branch_target, type});
  if (opt_entry.has_value()) {
    opt_entry->type = type;
    if (branch_target != 0)
      opt_entry->target = branch_target;
  }

  if (branch_target != 0) {
    ::L2_BTB.at(this).fill(opt_entry.value_or(::btb_entry_t{ip, branch_target, type}), 0);
  }
}
