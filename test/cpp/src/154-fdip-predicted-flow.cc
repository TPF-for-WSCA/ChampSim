#include <catch.hpp>

#include "../../../prefetcher/fdip/fdip.h"
#include "cache.h"
#include "defaults.hpp"
#include "instr.h"
#include "ooo_cpu.h"

namespace
{
struct flow_btb : champsim::modules::btb {
  using btb::btb;
  std::pair<champsim::address, bool> btb_prediction(champsim::address ip)
  {
    if (ip == champsim::address{0x100})
      return {champsim::address{0x200}, true};
    if (ip == champsim::address{0x200})
      return {champsim::address{0x400}, true};
    return {{}, false};
  }
};
}

TEST_CASE("FDIP walks predicted targets without consuming future trace or instruction history", "[fdip-flow]")
{
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1i}.prefetcher<fdip>()};
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.l1i(&cache).btb<flow_btb>()};
  cpu.input_queue.push_back(champsim::test::instruction_with_ip(0x100));
  cpu.input_queue.push_back(champsim::test::instruction_with_ip(0x300));
  cpu.advance_fdip();
  cpu.advance_fdip();
  cpu.advance_fdip();
  REQUIRE(cpu.input_queue.size() == 2);
  REQUIRE(cpu.wrong_path_history.empty());
  REQUIRE(cpu.fetch_target_queue.size() == 3);
  REQUIRE(cpu.fetch_target_queue[0].front().ip == champsim::address{0x100});
  REQUIRE(cpu.fetch_target_queue[1].front().ip == champsim::address{0x200});
  REQUIRE(cpu.fetch_target_queue[2].front().ip == champsim::address{0x400});
  REQUIRE(cpu.prediction_oracle == nullptr);
  REQUIRE(cpu.sim_stats.fdip_predictions == 10);
  REQUIRE_FALSE(cpu.consume_prediction(champsim::address{0x300}));
}

TEST_CASE("FDIP FTQ bounds prediction distance even through unseen sequential code", "[fdip-flow]")
{
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1i}.prefetcher<fdip>()};
  O3_CPU cpu{champsim::core_builder{}.l1i(&cache).fdip_ftq_size(2)};
  cpu.redirect_fdip(champsim::address{0x1000});
  for (int i = 0; i < 10; ++i)
    cpu.advance_fdip();
  REQUIRE(cpu.fetch_target_queue.size() == 2);
  REQUIRE(cpu.prediction_pc == champsim::address{0x1040});
  REQUIRE(cpu.sim_stats.fdip_ftq_full_cycles == 8);
  REQUIRE(cpu.sim_stats.fdip_predictions == 16);
  for (uint64_t ip = 0x1000; ip < 0x1040; ip += 4) {
    const auto prediction = cpu.consume_prediction(champsim::address{ip});
    REQUIRE(prediction.has_value());
    REQUIRE(prediction->ip == champsim::address{ip});
    if (ip == 0x101c)
      REQUIRE(cpu.fetch_target_queue.size() == 1);
  }
  REQUIRE(cpu.fetch_target_queue.empty());
  cpu.advance_fdip();
  REQUIRE(cpu.fetch_target_queue.front().front().ip == champsim::address{0x1040});
}

TEST_CASE("Dependency-only trace records neither consume predictions nor become replay fallthroughs", "[fdip-flow]")
{
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1i}.prefetcher<fdip>()};
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.l1i(&cache).wrong_path(true)};
  for (auto ip : {0x100u, 0x102u, 0x104u})
    cpu.input_queue.push_back(champsim::test::instruction_with_ip(ip));
  cpu.initialize_instruction();
  REQUIRE(cpu.input_queue.empty());
  REQUIRE(cpu.IFETCH_BUFFER.size() == 3);
  REQUIRE(cpu.fetch_target_queue.front().front().ip == champsim::address{0x108});
  REQUIRE(cpu.sim_stats.fdip_discontinuities == 0);
  REQUIRE(cpu.sim_stats.btb_lookups == 8); // Only the eight real PCs in the predicted block.
  REQUIRE(cpu.wrong_path_history.count(champsim::address{0x102}) == 0);
  REQUIRE(cpu.wrong_path_history.at(champsim::address{0x100}).fallthrough == champsim::address{0x104});
}

TEST_CASE("Unseen wrong-path NOPs follow BTB flow and recovery redirects after the configured delay", "[fdip-flow][wrong-path]")
{
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1i}.prefetcher<fdip>()};
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.l1i(&cache).btb<flow_btb>().wrong_path(true).mispredict_penalty(2)};
  cpu.warmup = false;
  auto branch = champsim::test::instruction_with_ip(0x100);
  branch.is_branch = true;
  branch.branch = BRANCH_INDIRECT;
  branch.branch_taken = true;
  branch.branch_target = champsim::address{0x300};
  cpu.input_queue.push_back(branch);
  cpu.input_queue.push_back(champsim::test::instruction_with_ip(0x300));
  cpu.initialize_instruction();
  REQUIRE(cpu.IFETCH_BUFFER.front().branch_mispredicted);
  cpu.initialize_instruction();
  REQUIRE(cpu.input_queue.size() == 1);
  REQUIRE(cpu.IFETCH_BUFFER.size() == 2);
  REQUIRE(cpu.IFETCH_BUFFER.back().ip == champsim::address{0x200});
  REQUIRE(cpu.IFETCH_BUFFER.back().wrong_path);
  REQUIRE(cpu.IFETCH_BUFFER.back().source_memory.empty());
  REQUIRE(cpu.wrong_path_next == champsim::address{0x400});
  REQUIRE(cpu.sim_stats.wrong_path_unknown == 1);
  cpu.do_complete_execution(cpu.IFETCH_BUFFER.front());
  REQUIRE_FALSE(cpu.wrong_path_active);
  REQUIRE(cpu.fetch_target_queue.empty());
  REQUIRE(cpu.prediction_pc == champsim::address{0x300});
  cpu.advance_fdip();
  REQUIRE(cpu.fetch_target_queue.empty());
  cpu.current_time = cpu.fetch_resume_time;
  cpu.initialize_instruction();
  REQUIRE(cpu.input_queue.empty());
  REQUIRE(cpu.IFETCH_BUFFER.back().ip == champsim::address{0x300});
  REQUIRE(cpu.sim_stats.branch_redirects == 1);
  REQUIRE(cpu.sim_stats.branch_redirect_cycles == 2);
}

TEST_CASE("FDIP ignores demand branch hooks and flushes only unissued targets on redirect", "[fdip-flow]")
{
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1i}};
  fdip prefetch{&cache};
  prefetch.prefetcher_branch_operate({}, BRANCH_DIRECT_JUMP, champsim::address{0xdead000});
  REQUIRE(prefetch.queued_targets.empty());
  prefetch.prefetcher_prediction_operate(champsim::address{0x1000});
  prefetch.prefetcher_prediction_operate(champsim::address{0x2000});
  prefetch.prefetcher_cycle_operate();
  REQUIRE(cache.sim_stats.pf_issued == 1);
  REQUIRE(prefetch.queued_targets.size() == 1);
  prefetch.prefetcher_redirect();
  REQUIRE(prefetch.queued_targets.empty());
  REQUIRE(cache.sim_stats.pf_issued == 1);
  REQUIRE(prefetch.recent_prefetches.size() == 1);
}

TEST_CASE("Wrong-path sequential flow does not follow a historical trace discontinuity", "[fdip-flow][wrong-path]")
{
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1i}.prefetcher<fdip>()};
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.l1i(&cache).wrong_path(true)};
  cpu.remember_instruction(champsim::test::instruction_with_ip(0x200));
  cpu.remember_instruction(champsim::test::instruction_with_ip(0x300));
  REQUIRE(cpu.wrong_path_history.at(champsim::address{0x200}).fallthrough == champsim::address{0x300});
  cpu.wrong_path_active = true;
  cpu.wrong_path_next = champsim::address{0x200};
  cpu.redirect_fdip(cpu.wrong_path_next);
  cpu.initialize_instruction();
  REQUIRE(cpu.IFETCH_BUFFER[1].ip == champsim::address{0x204});
  REQUIRE(cpu.wrong_path_next == champsim::address{0x218});
}

TEST_CASE("FDIP predicts nested returns using a speculative RAS and discards it on redirect", "[fdip-flow]")
{
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1i}.prefetcher<fdip>()};
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.l1i(&cache)};
  // Learn a completed nested call sequence, leaving the architectural RAS empty.
  cpu.impl_update_btb(champsim::address{0x100}, champsim::address{0x200}, true, BRANCH_DIRECT_CALL);
  cpu.impl_update_btb(champsim::address{0x200}, champsim::address{0x400}, true, BRANCH_DIRECT_CALL);
  cpu.impl_update_btb(champsim::address{0x400}, champsim::address{0x204}, true, BRANCH_RETURN);
  cpu.impl_update_btb(champsim::address{0x204}, champsim::address{0x104}, true, BRANCH_RETURN);
  cpu.redirect_fdip(champsim::address{0x100});
  for (int i = 0; i < 5; ++i)
    cpu.advance_fdip();
  REQUIRE(cpu.fetch_target_queue[2].front().target == champsim::address{0x204});
  REQUIRE(cpu.fetch_target_queue[3].front().target == champsim::address{0x104});
  REQUIRE(cpu.fetch_target_queue[4].front().ip == champsim::address{0x104});

  // Outstanding speculative calls must not survive a resolved redirect.
  cpu.redirect_fdip(champsim::address{0x100});
  cpu.advance_fdip();
  cpu.advance_fdip();
  cpu.redirect_fdip(champsim::address{0x204});
  cpu.advance_fdip();
  REQUIRE(cpu.fetch_target_queue.front().front().target == champsim::address{});
}
