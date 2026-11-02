#include <catch.hpp>

#include "../../../prefetcher/fdip/fdip.h"
#include "cache.h"
#include "defaults.hpp"
#include "instr.h"
#include "mocks.hpp"
#include "ooo_cpu.h"

namespace
{
struct replay_btb : champsim::modules::btb {
  using btb::btb;
  std::pair<champsim::address, bool> btb_prediction(champsim::address, uint8_t branch)
  {
    return branch == NOT_BRANCH ? std::pair{champsim::address{}, false} : std::pair{champsim::address{0x200}, true};
  }
};
}

TEST_CASE("TI-WP follows the local BTB without consuming the correct-path trace", "[wrong-path]")
{
  const bool uneven = GENERATE(false, true);
  auto builder = champsim::cache_builder{champsim::defaults::default_l1i};
  CACHE l1i{uneven ? builder.set_ubs() : builder.reset_ubs()};
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.wrong_path(true).l1i(&l1i).btb<replay_btb>()};
  cpu.warmup = false;
  auto known = champsim::test::instruction_with_ip(0x200);
  auto next = champsim::test::instruction_with_ip(0x204);
  cpu.remember_instruction(known);
  cpu.remember_instruction(next);
  cpu.previous_trace_instruction.reset();
  auto branch = champsim::test::instruction_with_ip(0x100);
  branch.is_branch = true;
  branch.branch = BRANCH_CONDITIONAL;
  branch.branch_taken = false;
  branch.instr_id = 10;
  cpu.input_queue.push_back(branch);
  cpu.input_queue.push_back(champsim::test::instruction_with_ip(0x104));
  cpu.initialize_instruction();
  REQUIRE(cpu.wrong_path_active);
  REQUIRE(cpu.IFETCH_BUFFER.size() == 1);
  cpu.initialize_instruction();
  REQUIRE(cpu.input_queue.size() == 1);
  REQUIRE(cpu.IFETCH_BUFFER.size() == 7);
  REQUIRE(cpu.IFETCH_BUFFER[1].ip == known.ip);
  REQUIRE(cpu.IFETCH_BUFFER[2].ip == next.ip);
  REQUIRE(cpu.IFETCH_BUFFER[1].wrong_path);
  REQUIRE(cpu.IFETCH_BUFFER[1].instr_id != known.instr_id);
  REQUIRE(cpu.sim_stats.wrong_path_fetched == 6);

  // Already-issued fetches may finish after squash, but their IDs are never reused.
  cpu.do_complete_execution(cpu.IFETCH_BUFFER.front());
  REQUIRE_FALSE(cpu.wrong_path_active);
  REQUIRE(cpu.IFETCH_BUFFER.size() == 1);
  REQUIRE(cpu.sim_stats.wrong_path_squashed == 6);
  REQUIRE(cpu.input_queue.front().ip == champsim::address{0x104});
}

TEST_CASE("TI-WP infers data operations at unknown PCs and enforces frontend capacity", "[wrong-path]")
{
  O3_CPU cpu{champsim::core_builder{}.wrong_path(true).ifetch_buffer_size(2).fetch_width(champsim::bandwidth::maximum_type{4})};
  cpu.remember_instruction(champsim::test::instruction_with_ip(0x200));
  cpu.remember_instruction(champsim::test::instruction_with_ip(0x204));
  cpu.remember_instruction(champsim::test::instruction_with_ip(0x200));
  cpu.wrong_path_active = true;
  cpu.wrong_path_next = champsim::address{0x200};
  cpu.initialize_instruction();
  REQUIRE(cpu.IFETCH_BUFFER.size() == 2);
  cpu.initialize_instruction();
  REQUIRE(cpu.IFETCH_BUFFER.size() == 2);
  cpu.recover_wrong_path();
  cpu.wrong_path_active = true;
  cpu.wrong_path_next = champsim::address{0x999};
  cpu.initialize_instruction();
  REQUIRE(cpu.IFETCH_BUFFER.size() == 2);
  REQUIRE(cpu.IFETCH_BUFFER.front().ip == champsim::address{0x999});
  REQUIRE(cpu.IFETCH_BUFFER.front().source_memory == std::vector{champsim::address{0x1000}});
  REQUIRE(cpu.IFETCH_BUFFER.front().destination_registers == std::vector{RegisterAllocator::INFERRED_LOAD_REGISTER});
  REQUIRE(cpu.IFETCH_BUFFER.back().ip == champsim::address{0x99d});
  REQUIRE(cpu.sim_stats.wrong_path_unknown == 2);
  REQUIRE(cpu.IFETCH_BUFFER.back().source_registers == cpu.IFETCH_BUFFER.front().destination_registers);
  REQUIRE(cpu.sim_stats.wrong_path_inferred_loads == 1);
  REQUIRE(cpu.sim_stats.wrong_path_inferred_arithmetic == 1);
}

TEST_CASE("TI-WP history is bounded and never infers a taken branch's fallthrough", "[wrong-path]")
{
  O3_CPU cpu{champsim::core_builder{}.wrong_path_history_size(2)};
  auto branch = champsim::test::instruction_with_ip(0x100);
  branch.is_branch = true;
  branch.branch_taken = true;
  cpu.remember_instruction(branch);
  cpu.remember_instruction(champsim::test::instruction_with_ip(0x200));
  REQUIRE(cpu.wrong_path_history.at(branch.ip).fallthrough == champsim::address{});
  cpu.remember_instruction(champsim::test::instruction_with_ip(0x204));
  REQUIRE(cpu.wrong_path_history.size() == 2);
  REQUIRE(cpu.wrong_path_history.count(branch.ip) == 0);
}

TEST_CASE("TI-WP keeps data operands in the backend without retirement", "[wrong-path]")
{
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.wrong_path(true)};
  auto instr = champsim::test::instruction_with_ip(0x200);
  instr.wrong_path = true;
  instr.source_memory.push_back(champsim::address{0x1000});
  instr.destination_memory.push_back(champsim::address{0x2000});
  instr.source_registers.push_back(1);
  instr.destination_registers.push_back(2);
  cpu.DISPATCH_BUFFER.push_back(instr);
  const auto free_registers = cpu.reg_allocator.count_free_registers();
  REQUIRE(cpu.dispatch_instruction() == 1);
  REQUIRE(cpu.ROB.size() == 1);
  REQUIRE(cpu.ROB.front().source_memory == instr.source_memory);
  REQUIRE(cpu.ROB.front().destination_memory == instr.destination_memory);
  REQUIRE(cpu.ROB.front().source_registers == instr.source_registers);
  REQUIRE(cpu.ROB.front().destination_registers == instr.destination_registers);
  REQUIRE(cpu.SQ.size() == 1);
  REQUIRE(std::count_if(cpu.LQ.begin(), cpu.LQ.end(), [](const auto& entry) { return entry.has_value(); }) == 1);
  cpu.ROB.front().ready_time = cpu.current_time;
  cpu.ROB.front().completed_mem_ops = cpu.ROB.front().num_mem_ops();
  REQUIRE(cpu.schedule_instruction() == 1);
  REQUIRE(cpu.execute_instruction() == 1);
  cpu.ROB.front().ready_time = cpu.current_time;
  REQUIRE(cpu.complete_inflight_instruction() == 1);
  REQUIRE(cpu.ROB.front().completed);
  REQUIRE(cpu.retire_rob() == 0);
  REQUIRE(cpu.num_retired == 0);
  REQUIRE(cpu.reg_allocator.count_free_registers() == free_registers - 2);
  cpu.wrong_path_active = true;
  cpu.recover_wrong_path();
  REQUIRE(cpu.ROB.empty());
  REQUIRE(cpu.SQ.empty());
  REQUIRE(std::none_of(cpu.LQ.begin(), cpu.LQ.end(), [](const auto& entry) { return entry.has_value(); }));
  REQUIRE(cpu.reg_allocator.count_free_registers() == free_registers);
  REQUIRE(cpu.sim_stats.wrong_path_squashed == 1);
  REQUIRE_FALSE(O3_CPU{champsim::core_builder{}}.WRONG_PATH);
}

TEST_CASE("TI-WP fills ROB capacity instead of stopping at the dispatch buffer", "[wrong-path]")
{
  champsim::channel fetch;
  do_nothing_MRC data;
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.wrong_path(true).rob_size(128).dispatch_buffer_size(4)
                 .decode_latency(0).dispatch_latency(0).schedule_latency(0).execute_latency(0).fetch_queues(&fetch).data_queues(&data.queues)};
  cpu.warmup = false;
  auto branch = champsim::test::instruction_with_ip(0x100);
  branch.instr_id = 1;
  branch.branch_mispredicted = true;
  branch.scheduled = true;
  branch.ready_time = champsim::chrono::clock::time_point::max();
  cpu.ROB.push_back(branch);
  cpu.fetch_resume_time = champsim::chrono::clock::time_point::max();
  cpu.wrong_path_active = true;
  cpu.wrong_path_next = champsim::address{0x200};
  for (unsigned cycle = 0; cycle < 100; ++cycle) {
    cpu.initialize_instruction();
    for (auto& instr : cpu.IFETCH_BUFFER)
      instr.fetch_completed = true;
    cpu.promote_to_decode();
    cpu.decode_instruction();
    cpu.dispatch_instruction();
    cpu.schedule_instruction();
    cpu.execute_instruction();
    cpu.current_time += cpu.clock_period;
    cpu.operate_lsq();
    data._operate();
    cpu.handle_memory_return();
    cpu.complete_inflight_instruction();
    REQUIRE(cpu.retire_rob() == 0);
  }
  REQUIRE(cpu.ROB.size() == cpu.ROB_SIZE);
  REQUIRE(cpu.sim_stats.wrong_path_fetched > 32);
  REQUIRE(cpu.ROB.front().instr_id == branch.instr_id);
  REQUIRE(cpu.ROB.back().completed);
  // Completing the older branch squashes the younger backend operations. Completion
  // iteration must remain valid while the ROB tail is erased.
  cpu.ROB.front().executed = true;
  cpu.ROB.front().ready_time = cpu.current_time;
  REQUIRE(cpu.complete_inflight_instruction() == 1);
  REQUIRE(cpu.ROB.size() == 1);
  REQUIRE(cpu.ROB.front().completed);
  REQUIRE(cpu.sim_stats.wrong_path_squashed == cpu.sim_stats.wrong_path_fetched);
  REQUIRE(cpu.retire_rob() == 1);
  REQUIRE(cpu.num_retired == 1);
}

TEST_CASE("TI-WP replay preserves logical operands but refreshes addresses and pipeline state", "[wrong-path]")
{
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.wrong_path(true)};
  auto known = champsim::test::instruction_with_ip(0x200);
  known.is_branch = true;
  known.branch = BRANCH_CONDITIONAL;
  known.source_registers.push_back(1);
  known.destination_registers.push_back(2);
  known.source_memory.push_back(champsim::address{0x1000});
  known.destination_memory.push_back(champsim::address{0x2000});
  known.completed = true;
  known.executed = true;
  known.scheduled = true;
  known.fetch_issued = true;
  known.fetch_completed = true;
  known.decoded = true;
  cpu.remember_instruction(known);
  cpu.wrong_path_active = true;
  cpu.wrong_path_next = known.ip;
  cpu.initialize_instruction();
  const auto& replay = cpu.IFETCH_BUFFER.front();
  REQUIRE(replay.is_branch);
  REQUIRE(replay.branch == known.branch);
  REQUIRE(replay.source_registers == known.source_registers);
  REQUIRE(replay.destination_registers == known.destination_registers);
  REQUIRE(replay.source_memory == std::vector{champsim::address{0x1000}});
  REQUIRE(replay.destination_memory == std::vector{champsim::address{0x1040}});
  REQUIRE_FALSE(replay.completed);
  REQUIRE_FALSE(replay.executed);
  REQUIRE_FALSE(replay.scheduled);
  REQUIRE_FALSE(replay.fetch_issued);
  REQUIRE_FALSE(replay.fetch_completed);
  REQUIRE_FALSE(replay.decoded);
}

TEST_CASE("TI-WP decode recovery squashes all frontend buffers", "[wrong-path]")
{
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.wrong_path(true)};
  auto branch = champsim::test::instruction_with_ip(0x100);
  branch.branch = BRANCH_DIRECT_JUMP;
  branch.branch_mispredicted = true;
  cpu.DECODE_BUFFER.push_back(branch);
  auto replay = champsim::test::instruction_with_ip(0x200);
  replay.wrong_path = true;
  replay.instr_id = uint64_t{1} << 63;
  cpu.IFETCH_BUFFER.push_back(replay);
  cpu.DIB_HIT_BUFFER.push_back(replay);
  cpu.DISPATCH_BUFFER.push_back(replay);
  cpu.wrong_path_active = true;
  cpu.fetch_resume_time = champsim::chrono::clock::time_point::max();
  cpu.decode_instruction();
  REQUIRE_FALSE(cpu.wrong_path_active);
  REQUIRE(cpu.IFETCH_BUFFER.empty());
  REQUIRE(cpu.DIB_HIT_BUFFER.empty());
  REQUIRE(cpu.DECODE_BUFFER.empty());
  REQUIRE(cpu.DISPATCH_BUFFER.size() == 1);
  REQUIRE_FALSE(cpu.DISPATCH_BUFFER.front().wrong_path);
  REQUIRE(cpu.sim_stats.wrong_path_squashed == 3);
  REQUIRE(cpu.fetch_resume_time != champsim::chrono::clock::time_point::max());
}

TEST_CASE("DIB hits resolve decoded branch target errors before execution", "[wrong-path][dib]")
{
  const auto type = GENERATE(BRANCH_DIRECT_JUMP, BRANCH_DIRECT_CALL, BRANCH_CONDITIONAL);
  const bool dib_hit = GENERATE(false, true);
  const bool replay = GENERATE(false, true);
  CACHE l1i{champsim::cache_builder{champsim::defaults::default_l1i}.prefetcher<fdip>()};
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.l1i(&l1i).wrong_path(replay)
                 .decode_latency(0).dib_hit_latency(0).dispatch_latency(0)};
  cpu.warmup = false;
  auto branch = champsim::test::instruction_with_ip(0x100);
  branch.instr_id = 1;
  branch.is_branch = true;
  branch.branch = type;
  branch.branch_taken = true;
  branch.branch_prediction = true; // Direction is right; the target was wrong.
  branch.branch_target = champsim::address{0x400};
  branch.branch_mispredicted = true;
  branch.fetch_completed = true;
  if (dib_hit)
    cpu.DIB.fill(branch.ip);
  cpu.do_check_dib(branch);
  REQUIRE(branch.decoded == dib_hit);
  cpu.IFETCH_BUFFER.push_back(branch);
  REQUIRE(cpu.promote_to_decode() == 1);
  REQUIRE(cpu.DIB_HIT_BUFFER.size() == (dib_hit ? 1 : 0));
  REQUIRE(cpu.DECODE_BUFFER.size() == (dib_hit ? 0 : 1));
  cpu.branch_recovery_start = cpu.current_time;
  cpu.fetch_resume_time = champsim::chrono::clock::time_point::max();
  cpu.wrong_path_active = replay;
  if (replay) {
    auto wrong = champsim::test::instruction_with_ip(0x200);
    wrong.wrong_path = true;
    wrong.instr_id = uint64_t{1} << 63;
    cpu.IFETCH_BUFFER.push_back(wrong);
  }

  REQUIRE(cpu.decode_instruction() == 1);
  REQUIRE(cpu.fetch_resume_time == cpu.current_time + cpu.BRANCH_MISPREDICT_PENALTY);
  REQUIRE(cpu.sim_stats.branch_redirects == 1);
  REQUIRE(cpu.sim_stats.fdip_redirects == 1);
  REQUIRE(cpu.prediction_pc == branch.branch_target);
  REQUIRE(cpu.prediction_recovery_wait);
  REQUIRE_FALSE(cpu.branch_recovery_start.has_value());
  REQUIRE_FALSE(cpu.wrong_path_active);
  REQUIRE(cpu.IFETCH_BUFFER.empty());
  REQUIRE(cpu.sim_stats.wrong_path_squashed == (replay ? 1 : 0));
  REQUIRE(cpu.DISPATCH_BUFFER.size() == 1);
  REQUIRE_FALSE(cpu.DISPATCH_BUFFER.front().branch_mispredicted);
  REQUIRE_FALSE(cpu.DISPATCH_BUFFER.front().executed);
  REQUIRE(cpu.dispatch_instruction() == 1);
  cpu.do_complete_execution(cpu.ROB.front());
  REQUIRE(cpu.sim_stats.branch_redirects == 1);
  REQUIRE(cpu.sim_stats.fdip_redirects == 1);
  REQUIRE(cpu.fetch_resume_time == cpu.current_time + cpu.BRANCH_MISPREDICT_PENALTY);
}

TEST_CASE("DIB hits retain execute-stage recovery for direction errors", "[wrong-path][dib]")
{
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.wrong_path(true).dib_hit_latency(0).dispatch_latency(0)};
  cpu.warmup = false;
  auto branch = champsim::test::instruction_with_ip(0x100);
  branch.branch = BRANCH_CONDITIONAL;
  branch.branch_taken = true;
  branch.branch_prediction = false;
  branch.branch_mispredicted = true;
  branch.instr_id = 1;
  cpu.DIB.fill(branch.ip);
  cpu.do_check_dib(branch);
  cpu.IFETCH_BUFFER.push_back(branch);
  REQUIRE(cpu.promote_to_decode() == 1);
  cpu.fetch_resume_time = champsim::chrono::clock::time_point::max();
  cpu.branch_recovery_start = cpu.current_time;
  cpu.wrong_path_active = true;
  REQUIRE(cpu.decode_instruction() == 1);
  REQUIRE(cpu.fetch_resume_time == champsim::chrono::clock::time_point::max());
  REQUIRE(cpu.sim_stats.branch_redirects == 0);
  REQUIRE(cpu.wrong_path_active);
  REQUIRE(cpu.DISPATCH_BUFFER.front().branch_mispredicted);
  REQUIRE(cpu.dispatch_instruction() == 1);
  cpu.do_complete_execution(cpu.ROB.front());
  REQUIRE(cpu.sim_stats.branch_redirects == 1);
  REQUIRE_FALSE(cpu.wrong_path_active);
}

TEST_CASE("Disabling TI-WP retains stall-on-misprediction behavior", "[wrong-path]")
{
  const bool uneven = GENERATE(false, true);
  auto builder = champsim::cache_builder{champsim::defaults::default_l1i};
  CACHE l1i{uneven ? builder.set_ubs() : builder.reset_ubs()};
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.wrong_path(false).l1i(&l1i).btb<replay_btb>()};
  cpu.warmup = false;
  auto branch = champsim::test::instruction_with_ip(0x100);
  branch.is_branch = true;
  branch.branch = BRANCH_CONDITIONAL;
  cpu.input_queue.push_back(branch);
  cpu.input_queue.push_back(champsim::test::instruction_with_ip(0x104));
  cpu.initialize_instruction();
  cpu.initialize_instruction();
  REQUIRE(cpu.IFETCH_BUFFER.size() == 1);
  REQUIRE(cpu.input_queue.size() == 1);
  REQUIRE(cpu.wrong_path_history.empty());
  REQUIRE_FALSE(cpu.wrong_path_active);
  REQUIRE(cpu.fetch_resume_time == champsim::chrono::clock::time_point::max());
}

TEST_CASE("TI-WP fetches use the instruction cache channel", "[wrong-path]")
{
  champsim::channel requests{8, 8, 8, champsim::data::bits{6}, false};
  CACHE l1i{champsim::cache_builder{champsim::defaults::default_l1i}};
  O3_CPU cpu{champsim::core_builder{}.wrong_path(true).fetch_queues(&requests).l1i(&l1i)};
  auto instr = champsim::test::instruction_with_ip(0x200);
  instr.wrong_path = true;
  instr.dib_checked = true;
  cpu.IFETCH_BUFFER.push_back(instr);
  REQUIRE(cpu.fetch_instruction() == 1);
  REQUIRE(requests.RQ.size() == 1);
  REQUIRE(requests.RQ.front().v_address == instr.ip);
}

TEST_CASE("Replayed branches follow local predictions instead of recorded outcomes", "[wrong-path]")
{
  CACHE l1i{champsim::cache_builder{champsim::defaults::default_l1i}};
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.wrong_path(true).l1i(&l1i).btb<replay_btb>()};
  auto branch = champsim::test::instruction_with_ip(0x300);
  branch.is_branch = true;
  branch.branch = BRANCH_CONDITIONAL;
  branch.branch_taken = true;
  branch.branch_target = champsim::address{0x400};
  cpu.remember_instruction(branch);
  cpu.remember_instruction(champsim::test::instruction_with_ip(0x200));
  cpu.wrong_path_active = true;
  cpu.wrong_path_next = branch.ip;
  cpu.initialize_instruction();
  REQUIRE(cpu.IFETCH_BUFFER.size() == 1); // taken prediction ends the fetch group
  REQUIRE(cpu.wrong_path_next == champsim::address{0x200});
  cpu.initialize_instruction();
  REQUIRE(cpu.IFETCH_BUFFER.size() == 7);
  REQUIRE(cpu.IFETCH_BUFFER[1].ip == champsim::address{0x200});
  REQUIRE(cpu.sim_stats.total_branch_types.value_or(BRANCH_CONDITIONAL, 0) == 0);
}
