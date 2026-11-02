#include "direct_predictor.h"

#include "instruction.h"
#include "storage.h"

namespace
{
uint8_t tag_width(std::size_t sets, std::optional<uint8_t> configured)
{
  const auto set_bits = champsim::storage_bits(sets);
  if (set_bits >= 62)
    throw std::invalid_argument("Basic BTB sets leave no address bits for tags");
  const auto full_width = static_cast<uint8_t>(62 - set_bits);
  const auto width = configured.value_or(full_width);
  if (width == 0 || width > full_width)
    throw std::invalid_argument("Basic BTB tag size must be positive and no larger than the full tag");
  return width;
}

uint64_t address_mask(uint64_t bits) { return bits == 64 ? ~uint64_t{0} : (uint64_t{1} << bits) - 1; }
} // namespace

direct_predictor::direct_predictor(std::size_t configured_sets, std::size_t configured_ways, std::optional<uint8_t> configured_tag_bits)
    : sets(configured_sets), ways(configured_ways), tag_bits(tag_width(sets, configured_tag_bits)),
      ip_mask(address_mask(2 + champsim::storage_bits(sets) + tag_bits)), BTB(sets, ways)
{
  if (ways == 0)
    throw std::invalid_argument("Basic BTB ways must be nonzero");
}

auto direct_predictor::check_hit(champsim::address ip, bool update_lru) -> std::optional<btb_entry_t>
{
  return BTB.check_hit({champsim::address{ip.to<uint64_t>() & ip_mask}, champsim::address{}, branch_info::ALWAYS_TAKEN}, update_lru);
}

void direct_predictor::update(champsim::address ip, champsim::address branch_target, uint8_t branch_type)
{
  // update btb entry
  auto type = branch_info::ALWAYS_TAKEN;
  if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL))
    type = branch_info::INDIRECT;
  else if (branch_type == BRANCH_RETURN)
    type = branch_info::RETURN;
  else if (branch_type == BRANCH_CONDITIONAL)
    type = branch_info::CONDITIONAL;

  // Keep the index and configured tag bits. Higher PC bits intentionally alias.
  const champsim::address key{ip.to<uint64_t>() & ip_mask};
  auto opt_entry = BTB.check_hit({key, branch_target, type});
  const bool is_call = branch_type == BRANCH_DIRECT_CALL || branch_type == BRANCH_INDIRECT_CALL;
  if (opt_entry.has_value()) {
    opt_entry->type = type;
    opt_entry->is_call = is_call;
    if (branch_target != champsim::address{})
      opt_entry->target = branch_target;
  }

  if (branch_target != champsim::address{}) {
    BTB.fill(opt_entry.value_or(btb_entry_t{key, branch_target, type, is_call}));
  }
}
