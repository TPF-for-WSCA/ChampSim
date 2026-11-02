#include <catch.hpp>

#include "../../../prefetcher/fdip/fdip.h"
#include "cache.h"
#include "defaults.hpp"
#include "instr.h"
#include "ooo_cpu.h"

namespace
{
struct dataflow_btb : champsim::modules::btb {
  using btb::btb;
  std::pair<champsim::address, bool> btb_prediction(champsim::address ip, uint8_t)
  {
    if (ip == champsim::address{0x10c})
      return {champsim::address{0x200}, true};
    if (ip == champsim::address{0x20c})
      return {champsim::address{0x300}, false};
    return {{}, false};
  }
};

auto replay_instruction(uint64_t id)
{
  auto instr = champsim::test::instruction_with_ip(0x400 + 4 * id);
  instr.instr_id = (uint64_t{1} << 63) + id;
  instr.wrong_path = true;
  return instr;
}
} // namespace

TEST_CASE("Unknown replay blocks infer loads arithmetic and stores from predicted boundaries", "[wrong-path][wrong-path-dataflow]")
{
  const bool use_fdip = GENERATE(false, true);
  CACHE plain{champsim::cache_builder{champsim::defaults::default_l1i}};
  CACHE fdip_cache{champsim::cache_builder{champsim::defaults::default_l1i}.prefetcher<fdip>()};
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.wrong_path(true).fetch_width(champsim::bandwidth::maximum_type{4})
                 .l1i(use_fdip ? &fdip_cache : &plain).btb<dataflow_btb>()};
  cpu.wrong_path_active = true;
  cpu.wrong_path_next = champsim::address{0x100};
  cpu.last_data_address = champsim::address{0x1000};
  if (use_fdip)
    cpu.redirect_fdip(cpu.wrong_path_next);
  cpu.initialize_instruction();
  REQUIRE(cpu.IFETCH_BUFFER.size() == 4);
  const auto& load = cpu.IFETCH_BUFFER[0];
  REQUIRE(load.source_memory == std::vector{champsim::address{0x1040}});
  REQUIRE(load.source_registers.empty());
  REQUIRE(load.destination_registers == std::vector{RegisterAllocator::INFERRED_LOAD_REGISTER});
  for (unsigned index : {1u, 2u}) {
    const auto& arithmetic = cpu.IFETCH_BUFFER[index];
    REQUIRE(arithmetic.num_mem_ops() == 0);
    REQUIRE(arithmetic.source_registers == load.destination_registers);
    REQUIRE(arithmetic.destination_registers == std::vector{RegisterAllocator::INFERRED_ALU_REGISTER});
  }
  const auto& store = cpu.IFETCH_BUFFER[3];
  REQUIRE(store.destination_memory == std::vector{champsim::address{0x1080}});
  REQUIRE(store.source_registers == cpu.IFETCH_BUFFER[2].destination_registers);
  REQUIRE(store.destination_registers.empty());
  REQUIRE(store.is_branch);
  REQUIRE(cpu.wrong_path_next == champsim::address{0x200});
  REQUIRE(cpu.wrong_path_history.empty());
  REQUIRE(cpu.sim_stats.wrong_path_inferred_loads == 1);
  REQUIRE(cpu.sim_stats.wrong_path_inferred_arithmetic == 2);
  REQUIRE(cpu.sim_stats.wrong_path_inferred_stores == 1);

  cpu.initialize_instruction();
  REQUIRE(cpu.IFETCH_BUFFER.size() == 8);
  REQUIRE(cpu.IFETCH_BUFFER[4].source_memory == std::vector{champsim::address{0x10c0}});
  REQUIRE(cpu.IFETCH_BUFFER[7].destination_memory == std::vector{champsim::address{0x1100}});
  REQUIRE(cpu.wrong_path_next == champsim::address{0x210}); // Predicted not-taken branch ends its block too.
  cpu.initialize_instruction();
  REQUIRE(cpu.IFETCH_BUFFER[8].source_memory == std::vector{champsim::address{0x1140}});
}

TEST_CASE("Replay address inference is seeded by successful real data accesses", "[wrong-path][wrong-path-dataflow]")
{
  const bool store = GENERATE(false, true);
  champsim::channel data;
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.wrong_path(true)
                 .fetch_width(champsim::bandwidth::maximum_type{1}).data_queues(&data)};
  LSQ_ENTRY access{champsim::address{0x9000}, 1, champsim::address{0x80}, {0, 0}};
  REQUIRE((store ? cpu.do_complete_store(access) : cpu.execute_load(access)));
  REQUIRE(cpu.last_data_address == access.virtual_address);
  cpu.wrong_path_active = true;
  cpu.wrong_path_next = champsim::address{0x100};
  cpu.initialize_instruction();
  REQUIRE(cpu.IFETCH_BUFFER.front().source_memory == std::vector{champsim::address{0x9040}});
  cpu.recover_wrong_path();
  access.virtual_address = champsim::address{0xa000};
  REQUIRE((store ? cpu.do_complete_store(access) : cpu.execute_load(access)));
  cpu.wrong_path_active = true;
  cpu.wrong_path_next = champsim::address{0x200};
  cpu.initialize_instruction();
  REQUIRE(cpu.IFETCH_BUFFER.front().source_memory == std::vector{champsim::address{0xa040}});
}

TEST_CASE("Single-slot inferred blocks use the terminal store rule", "[wrong-path][wrong-path-dataflow]")
{
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.wrong_path(true).btb<dataflow_btb>()};
  cpu.wrong_path_active = true;
  cpu.wrong_path_next = champsim::address{0x10c};
  cpu.initialize_instruction();
  REQUIRE(cpu.IFETCH_BUFFER.size() == 1);
  REQUIRE(cpu.IFETCH_BUFFER.front().destination_memory == std::vector{champsim::address{0x1000}});
  REQUIRE(cpu.sim_stats.wrong_path_inferred_loads == 0);
  REQUIRE(cpu.sim_stats.wrong_path_inferred_stores == 1);
  REQUIRE(cpu.wrong_path_next == champsim::address{0x200});
}

TEST_CASE("Inferred load ALU store dependencies execute through the backend and squash cleanly", "[wrong-path][wrong-path-dataflow]")
{
  champsim::channel fetch, data;
  CACHE l1i{champsim::cache_builder{champsim::defaults::default_l1i}};
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.wrong_path(true).register_file_size(8)
                 .fetch_width(champsim::bandwidth::maximum_type{4}).dispatch_width(champsim::bandwidth::maximum_type{4})
                 .schedule_latency(0).execute_latency(0).fetch_queues(&fetch).data_queues(&data).l1i(&l1i).btb<dataflow_btb>()};
  cpu.warmup = false;
  auto branch = champsim::test::instruction_with_ip(0x80);
  branch.instr_id = 1;
  branch.scheduled = true;
  branch.branch_mispredicted = true;
  branch.ready_time = champsim::chrono::clock::time_point::max();
  cpu.ROB.push_back(branch);
  cpu.wrong_path_active = true;
  cpu.wrong_path_next = champsim::address{0x100};
  cpu.last_data_address = champsim::address{0x8000};
  cpu.initialize_instruction();
  REQUIRE(cpu.IFETCH_BUFFER.size() == 4);
  cpu.DISPATCH_BUFFER.swap(cpu.IFETCH_BUFFER);
  REQUIRE(cpu.dispatch_instruction() == 4);
  REQUIRE(cpu.schedule_instruction() == 4);
  REQUIRE(cpu.ROB[2].source_registers == cpu.ROB[1].destination_registers);
  REQUIRE(cpu.ROB[3].source_registers == cpu.ROB[1].destination_registers);
  REQUIRE(cpu.ROB[4].source_registers == cpu.ROB[3].destination_registers);
  REQUIRE(cpu.execute_instruction() == 1); // Only the load is ready.
  cpu.current_time += cpu.clock_period;
  cpu.operate_lsq();
  REQUIRE(data.RQ.size() == 1);
  REQUIRE(data.RQ.front().v_address == champsim::address{0x8040});
  REQUIRE(cpu.execute_instruction() == 0);
  REQUIRE(cpu.complete_inflight_instruction() == 0); // A load needs its memory response.
  data.returned.emplace_back(data.RQ.front());
  data.RQ.clear();
  cpu.handle_memory_return();
  REQUIRE(cpu.complete_inflight_instruction() == 1);
  REQUIRE(cpu.execute_instruction() == 2); // Both ALUs depend on the latest load.
  REQUIRE(cpu.complete_inflight_instruction() == 2);
  REQUIRE(cpu.execute_instruction() == 1); // The store waits for the last ALU writer.
  cpu.current_time += cpu.clock_period;
  cpu.operate_lsq();
  REQUIRE(cpu.complete_inflight_instruction() == 1);
  REQUIRE(cpu.ROB.back().completed);
  REQUIRE(cpu.SQ.size() == 1);
  REQUIRE(cpu.SQ.front().fetch_issued);
  REQUIRE(data.WQ.empty()); // A speculative store executes but cannot commit a write.
  REQUIRE(cpu.sim_stats.wrong_path_dispatched == 4);
  REQUIRE(cpu.sim_stats.wrong_path_executed == 4);
  REQUIRE(cpu.sim_stats.wrong_path_loads_issued == 1);
  REQUIRE(cpu.sim_stats.wrong_path_stores_executed == 1);
  REQUIRE(cpu.retire_rob() == 0);
  cpu.ROB.front().executed = true;
  cpu.ROB.front().ready_time = cpu.current_time;
  REQUIRE(cpu.complete_inflight_instruction() == 1);
  REQUIRE(cpu.ROB.size() == 1);
  REQUIRE(cpu.SQ.empty());
  REQUIRE(std::none_of(cpu.LQ.begin(), cpu.LQ.end(), [](const auto& entry) { return entry.has_value(); }));
  REQUIRE(cpu.reg_allocator.count_free_registers() == 8);
  REQUIRE(cpu.sim_stats.wrong_path_squashed == 4);
  REQUIRE(cpu.retire_rob() == 1);
  REQUIRE(cpu.num_retired == 1);
}

TEST_CASE("Replay honors load and store queue capacity", "[wrong-path][wrong-path-dataflow]")
{
  const bool store = GENERATE(false, true);
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.wrong_path(true).lq_size(1).sq_size(1)};
  for (unsigned id : {1u, 2u}) {
    auto instr = replay_instruction(id);
    (store ? instr.destination_memory : instr.source_memory).push_back(champsim::address{0x1000});
    cpu.DISPATCH_BUFFER.push_back(instr);
  }
  REQUIRE(cpu.dispatch_instruction() == 1);
  REQUIRE(cpu.DISPATCH_BUFFER.size() == 1);
  REQUIRE(cpu.ROB.size() == 1);
  REQUIRE(cpu.dispatch_instruction() == 0);
  cpu.wrong_path_active = true;
  cpu.recover_wrong_path();
  REQUIRE(cpu.ROB.empty());
  REQUIRE(cpu.DISPATCH_BUFFER.empty());
}

TEST_CASE("Replay register rollback preserves older in-flight and committed producers", "[wrong-path][wrong-path-dataflow]")
{
  RegisterAllocator allocator{8};
  const auto initial = allocator.rename_src_register(1);
  const auto older = allocator.rename_dest_register(1, 1);
  allocator.rename_src_register(255); // A real trace register must not alias inference.
  const auto free_before = allocator.count_free_registers();
  allocator.begin_speculation();
  allocator.rename_dest_register(1, uint64_t{1} << 63);
  allocator.rename_src_register(200); // Source-only speculative allocations must also be reclaimed.
  allocator.rename_dest_register(RegisterAllocator::INFERRED_LOAD_REGISTER, (uint64_t{1} << 63) + 1);
  allocator.rename_dest_register(RegisterAllocator::INFERRED_ALU_REGISTER, (uint64_t{1} << 63) + 2);
  REQUIRE(allocator.count_free_registers() == free_before - 4);
  const bool retire_older = GENERATE(false, true);
  if (retire_older) {
    allocator.complete_dest_register(older);
    allocator.retire_dest_register(older);
    REQUIRE_FALSE(allocator.isValid(initial));
  }
  allocator.squash_speculation();
  REQUIRE(allocator.rename_src_register(1) == older);
  REQUIRE(allocator.isValid(older) == retire_older);
  REQUIRE_FALSE(allocator.isAllocated(200));
  REQUIRE_FALSE(allocator.isAllocated(RegisterAllocator::INFERRED_LOAD_REGISTER));
  REQUIRE_FALSE(allocator.isAllocated(RegisterAllocator::INFERRED_ALU_REGISTER));
  const auto expected_free = free_before + (retire_older ? 1 : 0);
  REQUIRE(allocator.count_free_registers() == expected_free);
  for (unsigned iteration = 0; iteration < 256; ++iteration) {
    allocator.begin_speculation();
    while (allocator.count_free_registers())
      allocator.rename_dest_register(RegisterAllocator::INFERRED_ALU_REGISTER, uint64_t{1} << 63);
    allocator.squash_speculation();
    allocator.squash_speculation(); // Recovery is idempotent.
    REQUIRE(allocator.count_free_registers() == expected_free);
    REQUIRE(allocator.rename_src_register(1) == older);
  }
}

TEST_CASE("Replay cannot rename ahead of an older unready correct-path instruction", "[wrong-path][wrong-path-dataflow]")
{
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.wrong_path(true)};
  auto older = champsim::test::instruction_with_ip(0x100);
  older.destination_registers = {1};
  older.ready_time = champsim::chrono::clock::time_point::max();
  auto replay = replay_instruction(1);
  replay.destination_registers = {1};
  cpu.ROB = {older, replay};
  REQUIRE(cpu.schedule_instruction() == 0);
  REQUIRE_FALSE(cpu.reg_allocator.isAllocated(1));
  cpu.ROB.front().ready_time = cpu.current_time;
  REQUIRE(cpu.schedule_instruction() == 2);
  const auto older_physical = cpu.ROB.front().destination_registers.front();
  cpu.wrong_path_active = true;
  cpu.recover_wrong_path();
  REQUIRE(cpu.ROB.size() == 1);
  REQUIRE(cpu.reg_allocator.rename_src_register(1) == older_physical);
}

TEST_CASE("Scheduler rechecks do not interpret renamed physical IDs as logical registers", "[wrong-path-dataflow][register-rename]")
{
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.register_file_size(512)
                 .schedule_width(champsim::bandwidth::maximum_type{512})};
  for (unsigned id = 1; id <= 300; ++id) {
    auto instr = champsim::test::instruction_with_ip(4 * id);
    instr.instr_id = id;
    instr.source_registers = {1};
    instr.destination_registers = {1};
    cpu.ROB.push_back(instr);
  }
  REQUIRE(cpu.schedule_instruction() == 300);
  REQUIRE(cpu.ROB.back().source_registers.front() > RegisterAllocator::INFERRED_ALU_REGISTER);
  REQUIRE(cpu.schedule_instruction() == 0);
}

TEST_CASE("A full physical register file does not block renaming a register-free consumer", "[wrong-path-dataflow][register-rename]")
{
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.register_file_size(2)};
  for (unsigned id : {1u, 2u}) {
    auto producer = champsim::test::instruction_with_ip(4 * id);
    producer.instr_id = id;
    producer.destination_registers = {cpu.reg_allocator.rename_dest_register(1, id)};
    producer.scheduled = true;
    producer.executed = true;
    cpu.ROB.push_back(producer);
  }
  auto consumer = champsim::test::instruction_with_ip(12);
  consumer.instr_id = 3;
  consumer.source_registers = {1};
  cpu.ROB.push_back(consumer);
  REQUIRE(cpu.reg_allocator.count_free_registers() == 0);
  REQUIRE(cpu.schedule_instruction() == 1);
  REQUIRE(cpu.ROB.back().source_registers == cpu.ROB[1].destination_registers);
}

TEST_CASE("Replay squash removes forwarding links before load queue slot reuse", "[wrong-path][wrong-path-dataflow]")
{
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.wrong_path(true).lq_size(1)};
  auto store = champsim::test::instruction_with_ip(0x100);
  store.instr_id = 1;
  store.destination_memory = {champsim::address{0x4000}};
  auto replay = replay_instruction(1);
  replay.source_memory = store.destination_memory;
  cpu.DISPATCH_BUFFER = {store, replay};
  REQUIRE(cpu.dispatch_instruction() == 2);
  REQUIRE(cpu.SQ.front().lq_depend_on_me.size() == 1);
  cpu.wrong_path_active = true;
  cpu.recover_wrong_path();
  REQUIRE(cpu.SQ.size() == 1);
  REQUIRE(cpu.SQ.front().lq_depend_on_me.empty());
  REQUIRE_FALSE(cpu.LQ.front().has_value());
  auto correct_load = champsim::test::instruction_with_ip(0x104);
  correct_load.instr_id = 2;
  correct_load.source_memory = store.destination_memory;
  cpu.DISPATCH_BUFFER.push_back(correct_load);
  REQUIRE(cpu.dispatch_instruction() == 1);
  REQUIRE(cpu.SQ.front().lq_depend_on_me.size() == 1);
  cpu.do_finish_store(cpu.SQ.front());
  REQUIRE(cpu.ROB.back().completed_mem_ops == 1);
  REQUIRE_FALSE(cpu.LQ.front().has_value());
}

TEST_CASE("Squashed loads leave cache requests in flight without retaining backend state", "[wrong-path][wrong-path-dataflow]")
{
  champsim::channel fetch, data;
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.wrong_path(true).lq_size(1)
                 .schedule_latency(0).execute_latency(0).fetch_queues(&fetch).data_queues(&data)};
  auto replay = replay_instruction(1);
  replay.source_memory = {champsim::address{0x4000}};
  replay.destination_registers = {RegisterAllocator::INFERRED_LOAD_REGISTER};
  cpu.DISPATCH_BUFFER.push_back(replay);
  REQUIRE(cpu.dispatch_instruction() == 1);
  REQUIRE(cpu.schedule_instruction() == 1);
  REQUIRE(cpu.execute_instruction() == 1);
  cpu.current_time += cpu.clock_period;
  cpu.operate_lsq();
  REQUIRE(data.RQ.size() == 1);
  cpu.wrong_path_active = true;
  cpu.recover_wrong_path();
  REQUIRE(cpu.ROB.empty());
  REQUIRE_FALSE(cpu.LQ.front().has_value());
  REQUIRE(cpu.reg_allocator.count_free_registers() == cpu.REGISTER_FILE_SIZE);
  REQUIRE(data.RQ.size() == 1);
  data.returned.emplace_back(data.RQ.front());
  cpu.handle_memory_return(); // A late response to a squashed load is harmless.
  REQUIRE(data.returned.empty());
  REQUIRE(cpu.num_retired == 0);
}
