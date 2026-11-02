
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
  // Replay and FDIP lookups during branch recovery must not promote BTB entries.
  auto btb_entry = direct.check_hit(ip, !intern_ || (!intern_->wrong_path_active && !intern_->branch_recovery_start.has_value()));

  if (intern_) {
    ++intern_->sim_stats.btb_lookups;
    intern_->sim_stats.btb_hits += btb_entry.has_value();
    if (intern_->IDEAL_BTB) {
      auto found = ideal_entries.find(ip);
      if (found != ideal_entries.end())
        btb_entry = found->second;
    }
  }

  const auto* oracle = intern_ ? intern_->prediction_oracle : nullptr;
  const bool lookahead = intern_ && intern_->uses_fdip() && !oracle;
  if (lookahead) {
    const auto block_id = std::pair{intern_->sim_stats.fdip_fetch_blocks, intern_->sim_stats.begin_instrs};
    if (block_id != speculative_block) {
      // An empty FTQ is a checkpoint only outside unresolved recovery. The
      // predictor also walks wrong paths when replay is disabled.
      if (intern_->fetch_target_queue.empty() && !intern_->wrong_path_active && !intern_->branch_recovery_start.has_value())
        speculative_ras = ras;
      speculative_block = block_id;
    }
  }
  if (oracle && oracle->is_branch && intern_->IDEAL_BTB) {
    using info = direct_predictor::branch_info;
    const auto type = (oracle->branch == BRANCH_CONDITIONAL || oracle->branch == BRANCH_OTHER) ? info::CONDITIONAL
                    : oracle->branch == BRANCH_RETURN ? info::RETURN
                    : (oracle->branch == BRANCH_INDIRECT || oracle->branch == BRANCH_INDIRECT_CALL) ? info::INDIRECT
                    : info::ALWAYS_TAKEN;
    btb_entry = direct_predictor::btb_entry_t{ip, oracle->branch_target, type,
                                           oracle->branch == BRANCH_DIRECT_CALL || oracle->branch == BRANCH_INDIRECT_CALL};
  }

  if (intern_)
    intern_->prediction_btb_hit = btb_entry.has_value();

  // no prediction for this IP
  if (!btb_entry.has_value())
    return {champsim::address{}, false};

  if (btb_entry->type == direct_predictor::branch_info::RETURN) {
    if (oracle && intern_->IDEAL_BTB)
      return {oracle->branch_target, true};
    auto& stack = lookahead ? speculative_ras : ras;
    auto prediction = stack.prediction();
    if (lookahead && !stack.stack.empty())
      stack.stack.pop_back();
    return prediction;
  }

  if (btb_entry->type == direct_predictor::branch_info::INDIRECT) {
    auto prediction = oracle && intern_->IDEAL_IBP ? std::pair{oracle->branch_target, true} : indirect.prediction(ip);
    if (lookahead && btb_entry->is_call && prediction.first != champsim::address{})
      speculative_ras.push(ip);
    return prediction;
  }

  if (lookahead && btb_entry->is_call)
    speculative_ras.push(ip);

  return {btb_entry->target, btb_entry->type != direct_predictor::branch_info::CONDITIONAL};
}

void basic_btb::update_btb(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  // add something to the RAS
  if (branch_type == BRANCH_DIRECT_CALL || branch_type == BRANCH_INDIRECT_CALL)
    ras.push(ip);

  // Train indirect targets with the preceding history, then advance history
  // for every real branch. Prediction-only wrong-path fetches do not train.
  indirect.update(ip, branch_target, taken, branch_type);

  if (branch_type == BRANCH_RETURN)
    ras.calibrate_call_size(branch_target);

  direct.update(ip, branch_target, branch_type);
  if (intern_ && intern_->IDEAL_BTB && branch_target != champsim::address{}) {
    using info = direct_predictor::branch_info;
    const auto type = (branch_type == BRANCH_CONDITIONAL || branch_type == BRANCH_OTHER) ? info::CONDITIONAL
                    : branch_type == BRANCH_RETURN ? info::RETURN
                    : (branch_type == BRANCH_INDIRECT || branch_type == BRANCH_INDIRECT_CALL) ? info::INDIRECT
                    : info::ALWAYS_TAKEN;
    ideal_entries[ip] = {ip, branch_target, type, branch_type == BRANCH_DIRECT_CALL || branch_type == BRANCH_INDIRECT_CALL};
  }
}

void basic_btb::initialize_btb()
{
  const uint64_t count = direct.sets * direct.ways;
  champsim::print_storage("CPU " + std::to_string(intern_ ? intern_->cpu : 0) + " basic_btb", {
      {"Direct BTB targets", count * 64},
      {"Direct BTB tags", count * (64 - 2 - champsim::storage_bits(direct.sets))},
      {"Direct BTB valid bits", count}, {"Direct BTB branch types/call flags", count * 3},
      {"Direct BTB replacement (LRU ranks)", count * champsim::storage_bits(direct.ways)},
      {"ITTAGE base targets/valid", ittage::base_storage_bits},
      {"ITTAGE tagged targets/tags/confidence/useful/valid", ittage::tagged_storage_bits},
      {"ITTAGE history/folds/cursor/chooser/aging", ittage::history_storage_bits},
      {"Return address stack", return_stack::max_size * 64},
      {"Speculative return stack checkpoint", intern_ && intern_->uses_fdip() ? return_stack::max_size * 64 : 0},
      {"Return stack pointer", champsim::storage_bits(return_stack::max_size + 1)},
      {"Call-size trackers", return_stack::num_call_size_trackers * 64}});
}
