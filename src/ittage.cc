#include "ittage.h"

#include <algorithm>

#include "instruction.h"

namespace
{
uint64_t mix_pc(champsim::address ip)
{
  auto value = ip.to<uint64_t>() >> 2;
  value ^= value >> 21;
  value ^= value >> 11;
  return value;
}

uint16_t fold_next(uint16_t value, unsigned width, std::size_t length, bool incoming, bool outgoing)
{
  const auto mask = (1u << width) - 1;
  const auto rotated = ((static_cast<unsigned>(value) << 1) | (value >> (width - 1))) & mask;
  return static_cast<uint16_t>(rotated ^ static_cast<unsigned>(incoming) ^ (static_cast<unsigned>(outgoing) << (length % width)));
}
} // namespace

ittage::lookup_result ittage::lookup(champsim::address ip) const
{
  lookup_result result{};
  const auto pc = mix_pc(ip);
  result.base_index = pc & (base_entries - 1);
  for (std::size_t bank = 0; bank < tables.size(); ++bank) {
    result.indices[bank] = (pc ^ (pc >> (bank + 1)) ^ index_history[bank]) & (tagged_entries - 1);
    result.tags[bank] = static_cast<uint16_t>(((pc >> 10) ^ tag_history[bank] ^ (alternate_tag_history[bank] << 1)) & ((1u << tag_bits) - 1));
    const auto& entry = tables[bank][result.indices[bank]];
    if (entry.valid && entry.tag == result.tags[bank]) {
      result.alternate = result.provider;
      result.provider = static_cast<int>(bank);
    }
  }
  if (result.alternate >= 0)
    result.alternate_target = tables[static_cast<std::size_t>(result.alternate)][result.indices[static_cast<std::size_t>(result.alternate)]].target;
  else if (base[result.base_index].valid)
    result.alternate_target = base[result.base_index].target;
  result.target = result.alternate_target;
  if (result.provider >= 0) {
    const auto& provider = tables[static_cast<std::size_t>(result.provider)][result.indices[static_cast<std::size_t>(result.provider)]];
    if (provider.confidence > 0 || use_alternate < 0 || result.alternate_target == champsim::address{})
      result.target = provider.target;
  }
  return result;
}

std::pair<champsim::address, bool> ittage::prediction(champsim::address ip) const { return {lookup(ip).target, true}; }

void ittage::train(champsim::address ip, champsim::address target)
{
  const auto result = lookup(ip);
  bool provider_correct = false;
  if (result.provider >= 0) {
    auto& provider = tables[static_cast<std::size_t>(result.provider)][result.indices[static_cast<std::size_t>(result.provider)]];
    provider_correct = provider.target == target;
    const bool alternate_correct = result.alternate_target == target;
    if (provider.confidence == 0 && provider_correct != alternate_correct)
      use_alternate = static_cast<int8_t>(std::clamp(static_cast<int>(use_alternate) + (alternate_correct ? 1 : -1), -8, 7));
    if (result.target == target && !alternate_correct)
      provider.useful = true;
    if (provider_correct)
      provider.confidence = std::min<uint8_t>(3, static_cast<uint8_t>(provider.confidence + 1));
    else if (provider.confidence > 0)
      --provider.confidence;
    else
      provider.target = target;
  }
  // A missed final prediction needs a longer context, unless the weak provider
  // already had the correct target and the alternate chooser overrode it.
  if (result.target != target && !provider_correct) {
    for (std::size_t bank = static_cast<std::size_t>(result.provider + 1); bank < tables.size(); ++bank) {
      auto& entry = tables[bank][result.indices[bank]];
      if (!entry.valid || !entry.useful) {
        entry = tagged_entry{target, result.tags[bank], 0, false, true};
        if (allocation_pressure > 0)
          --allocation_pressure;
        break;
      }
      if (allocation_pressure == 255) {
        for (auto& table : tables)
          for (auto& candidate : table)
            candidate.useful = false;
        allocation_pressure = 0;
      } else {
        ++allocation_pressure;
      }
    }
  }
  base[result.base_index] = base_entry{target, true};
}

void ittage::push_history(bool bit)
{
  for (std::size_t bank = 0; bank < tables.size(); ++bank) {
    const bool outgoing = history[(history_head + history_lengths[bank] - 1) % history.size()];
    index_history[bank] = fold_next(index_history[bank], 10, history_lengths[bank], bit, outgoing);
    tag_history[bank] = fold_next(tag_history[bank], 11, history_lengths[bank], bit, outgoing);
    alternate_tag_history[bank] = fold_next(alternate_tag_history[bank], 10, history_lengths[bank], bit, outgoing);
  }
  history_head = (history_head + history.size() - 1) % history.size();
  history[history_head] = bit;
}

void ittage::update_history(champsim::address ip, champsim::address target, bool taken, uint8_t branch_type)
{
  // Indirect targets and call sites contribute path information; all other
  // branches contribute one direction bit. Histories advance only on updates.
  unsigned bits = 1;
  uint64_t value = taken;
  if (taken && (branch_type == BRANCH_INDIRECT || branch_type == BRANCH_INDIRECT_CALL || branch_type == BRANCH_DIRECT_CALL)) {
    bits = branch_type == BRANCH_DIRECT_CALL ? 5 : 10;
    value = mix_pc(ip) ^ (target.to<uint64_t>() >> 2) ^ (target.to<uint64_t>() >> 11);
  }
  for (unsigned i = 0; i < bits; ++i)
    push_history(((value >> i) & 1) != 0);
}

void ittage::update(champsim::address ip, champsim::address target, bool taken, uint8_t branch_type)
{
  if (taken && target != champsim::address{} && (branch_type == BRANCH_INDIRECT || branch_type == BRANCH_INDIRECT_CALL))
    train(ip, target);
  update_history(ip, target, taken, branch_type);
}
