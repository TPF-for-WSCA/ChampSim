#include <catch.hpp>

#include "instr.h"
#include "ooo_cpu.h"

namespace
{
using event = champsim::backend_event;

void observe(O3_CPU& cpu)
{
  cpu.reg_allocator.set_energy_activity(cpu.backend_activity.emplace());
}

ooo_model_instr instruction(uint64_t id)
{
  auto instr = champsim::test::instruction_with_ip(0x1000 + 4 * id);
  instr.instr_id = id;
  return instr;
}
} // namespace

TEST_CASE("Backend accounting follows a two-instruction register dependency")
{
  O3_CPU cpu{champsim::core_builder{}.rob_size(2).register_file_size(8).schedule_width(champsim::bandwidth::maximum_type{2})};
  observe(cpu);
  auto producer = instruction(1);
  producer.destination_registers = {5};
  auto consumer = instruction(2);
  consumer.source_registers = {5};
  consumer.destination_registers = {5};
  cpu.ROB = {producer, consumer};

  REQUIRE(cpu.schedule_instruction() == 2);
  REQUIRE(cpu.execute_instruction() == 1);
  CHECK(cpu.execute_instruction() == 0);
  REQUIRE(cpu.complete_inflight_instruction() == 1);
  REQUIRE(cpu.execute_instruction() == 1);
  REQUIRE(cpu.complete_inflight_instruction() == 1);
  REQUIRE(cpu.retire_rob() == 1);
  REQUIRE(cpu.retire_rob() == 1);
  const auto& energy = *cpu.backend_activity;
  CHECK(energy[event::rat_read] == 1);
  CHECK(energy[event::rat_write] == 2);
  CHECK(energy[event::ready_read] == 2);
  CHECK(energy[event::operand_read] == 1);
  CHECK(energy[event::operand_write] == 2);
  CHECK(energy[event::free_put] == 1);
  CHECK(energy[event::rob_retire] == 2);
}

TEST_CASE("Backend accounting distinguishes forwarding from cache requests and full-queue stalls")
{
  O3_CPU cpu{champsim::core_builder{}.rob_size(3).sq_size(1).lq_size(1)};
  observe(cpu);
  auto store = instruction(1);
  store.destination_memory = {champsim::address{0x8000}};
  auto load = instruction(2);
  load.source_memory = store.destination_memory;
  auto second_load = instruction(3);
  second_load.source_memory = {champsim::address{0x9000}};
  cpu.DISPATCH_BUFFER = {store, load, second_load};

  REQUIRE(cpu.dispatch_instruction() == 1);
  REQUIRE(cpu.dispatch_instruction() == 1);
  REQUIRE(cpu.dispatch_instruction() == 0);
  REQUIRE(cpu.LQ.front().has_value());
  REQUIRE(cpu.SQ.size() == 1);
  const auto& energy = *cpu.backend_activity;
  CHECK(energy[event::rob_in] == 2);
  CHECK(energy[event::store_in] == 1);
  CHECK(energy[event::load_in] == 1);
  CHECK(energy[event::store_match] == 1);
  CHECK(energy[event::forward_wait] == 1);

  cpu.do_execution(cpu.ROB.front());
  cpu.operate_lsq();
  CHECK_FALSE(cpu.LQ.front().has_value());
  CHECK(energy[event::forward_data] == 1);
  CHECK(energy[event::load_release] == 1);
  CHECK(energy[event::load_send] == 0);
  CHECK(energy[event::rob_memory_done] == 2);

  REQUIRE(cpu.dispatch_instruction() == 1);
  cpu.DISPATCH_BUFFER.push_back(instruction(4));
  CHECK(cpu.dispatch_instruction() == 0);
  CHECK(energy[event::load_in] == 2);
  CHECK(energy[event::rob_in] == 3);
}

TEST_CASE("Backend accounting restores a replay checkpoint and releases only squashed entries")
{
  O3_CPU cpu{champsim::core_builder{}.rob_size(2).register_file_size(8)};
  observe(cpu);
  auto replay = instruction(2);
  replay.wrong_path = true;
  replay.destination_registers = {2};
  cpu.ROB = {instruction(1), replay};
  cpu.wrong_path_active = true;

  cpu.do_scheduling(cpu.ROB.back());
  cpu.recover_wrong_path();
  REQUIRE(cpu.ROB.size() == 1);
  CHECK(cpu.ROB.front().instr_id == 1);
  const auto& energy = *cpu.backend_activity;
  CHECK(energy[event::checkpoint] == 1);
  CHECK(energy[event::restore] == 1);
  CHECK(energy[event::rob_release] == 1);
  CHECK(energy[event::free_put] == 1);
  CHECK(energy[event::rob_retire] == 0);
}

TEST_CASE("On-chip wire counters count admitted traffic and consumed responses")
{
  champsim::channel link{1, 1, 1, champsim::data::bits{6}, false};
  auto& counts = link.link_activity.emplace();
  REQUIRE(link.add_rq({}));
  CHECK_FALSE(link.add_rq({}));
  REQUIRE(link.add_pq({}));
  REQUIRE(link.add_wq({}));
  CHECK_FALSE(link.add_wq({}));
  link.record_responses(2);
  CHECK(counts.requests == 2);
  CHECK(counts.writes == 1);
  CHECK(counts.responses == 2);
}
