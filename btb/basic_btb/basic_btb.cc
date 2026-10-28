
/*
 * This file implements a basic Branch Target Buffer (BTB) structure.
 * It uses a set-associative BTB to predict the targets of non-return branches,
 * and it uses a small Return Address Stack (RAS) to predict the target of
 * returns.
 */

// Core headers include legacy compatibility headers; compile this bridge
// outside the module-only header restrictions.
#undef CHAMPSIM_MODULE
#include "ooo_cpu.h"
#include "basic_btb.h"
#include "storage.h"

#include "instruction.h"

basic_btb::basic_btb(O3_CPU* cpu)
    : btb(cpu), direct(cpu ? cpu->BTB_SETS : 1024, cpu ? cpu->BTB_WAYS : 8)
{
}

std::pair<champsim::address, bool> basic_btb::btb_prediction(champsim::address ip)
{
  // use BTB for all other branches + direct calls
  auto btb_entry = direct.check_hit(ip);

  // no prediction for this IP
  if (!btb_entry.has_value())
    return {champsim::address{}, false};

  if (btb_entry->type == direct_predictor::branch_info::RETURN)
    return ras.prediction();

  if (btb_entry->type == direct_predictor::branch_info::INDIRECT)
    return indirect.prediction(ip);

  return {btb_entry->target, btb_entry->type != direct_predictor::branch_info::CONDITIONAL};
}

void basic_btb::update_btb(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  // add something to the RAS
  if (branch_type == BRANCH_DIRECT_CALL || branch_type == BRANCH_INDIRECT_CALL)
    ras.push(ip);

  // updates for indirect branches
  if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL))
    indirect.update_target(ip, branch_target);

  if (branch_type == BRANCH_CONDITIONAL)
    indirect.update_direction(taken);

  if (branch_type == BRANCH_RETURN)
    ras.calibrate_call_size(branch_target);

  direct.update(ip, branch_target, branch_type);
}

void basic_btb::initialize_btb()
{
  const uint64_t count = direct.sets * direct.ways;
  champsim::print_storage("CPU " + std::to_string(intern_ ? intern_->cpu : 0) + " basic_btb", {
      {"Direct BTB targets", count * 64},
      {"Direct BTB tags", count * (64 - 2 - champsim::storage_bits(direct.sets))},
      {"Direct BTB valid bits", count}, {"Direct BTB branch types", count * 2},
      {"Direct BTB replacement (LRU ranks)", count * champsim::storage_bits(direct.ways)},
      {"Indirect target table", indirect_predictor::size * 64},
      {"Conditional history", champsim::storage_bits(indirect_predictor::size)},
      {"Return address stack", return_stack::max_size * 64},
      {"Return stack pointer", champsim::storage_bits(return_stack::max_size + 1)},
      {"Call-size trackers", return_stack::num_call_size_trackers * 64}});
}
