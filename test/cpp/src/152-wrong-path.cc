#include <catch.hpp>

#include "cache.h"
#include "defaults.hpp"
#include "instr.h"
#include "ooo_cpu.h"

namespace
{
struct replay_btb : champsim::modules::btb {
  using btb::btb;
  std::pair<champsim::address, bool> btb_prediction(champsim::address, uint8_t) { return {champsim::address{0x200}, true}; }
};
}

TEST_CASE("TI-WP follows the local BTB without consuming the correct-path trace", "[wrong-path]")
{
  CACHE l1i{champsim::cache_builder{champsim::defaults::default_l1i}};
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
  REQUIRE(cpu.IFETCH_BUFFER.size() == 3);
  REQUIRE(cpu.IFETCH_BUFFER[1].ip == known.ip);
  REQUIRE(cpu.IFETCH_BUFFER[2].ip == next.ip);
  REQUIRE(cpu.IFETCH_BUFFER[1].wrong_path);
  REQUIRE(cpu.IFETCH_BUFFER[1].instr_id != known.instr_id);
  REQUIRE(cpu.sim_stats.wrong_path_fetched == 2);

  // Already-issued fetches may finish after squash, but their IDs are never reused.
  cpu.do_complete_execution(cpu.IFETCH_BUFFER.front());
  REQUIRE_FALSE(cpu.wrong_path_active);
  REQUIRE(cpu.IFETCH_BUFFER.size() == 1);
  REQUIRE(cpu.sim_stats.wrong_path_squashed == 2);
  REQUIRE(cpu.input_queue.front().ip == champsim::address{0x104});
}

TEST_CASE("TI-WP stops at unknown PCs and enforces frontend capacity", "[wrong-path]")
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
  REQUIRE(cpu.IFETCH_BUFFER.empty());
  REQUIRE(cpu.sim_stats.wrong_path_unknown == 1);
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

TEST_CASE("TI-WP cannot dispatch data operations or retire", "[wrong-path]")
{
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.wrong_path(true)};
  auto instr = champsim::test::instruction_with_ip(0x200);
  instr.wrong_path = true;
  instr.source_memory.push_back(champsim::address{0x1000});
  instr.destination_memory.push_back(champsim::address{0x2000});
  cpu.DISPATCH_BUFFER.push_back(instr);
  REQUIRE(cpu.dispatch_instruction() == 0);
  REQUIRE(cpu.ROB.empty());
  REQUIRE(cpu.SQ.empty());
  REQUIRE(cpu.num_retired == 0);
  REQUIRE_FALSE(O3_CPU{champsim::core_builder{}}.WRONG_PATH);
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

TEST_CASE("Disabling TI-WP retains stall-on-misprediction behavior", "[wrong-path]")
{
  CACHE l1i{champsim::cache_builder{champsim::defaults::default_l1i}};
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
  REQUIRE(cpu.IFETCH_BUFFER.size() == 2);
  REQUIRE(cpu.IFETCH_BUFFER.back().ip == champsim::address{0x200});
  REQUIRE(cpu.sim_stats.total_branch_types.value_or(BRANCH_CONDITIONAL, 0) == 0);
}
