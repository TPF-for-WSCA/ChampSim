#include <catch.hpp>

#include "cache.h"
#include "defaults.hpp"
#include "instruction.h"
#include "../../../prefetcher/fdip/fdip.h"

TEST_CASE("FDIP skips resident virtual targets but issues missing targets", "[fdip-presence]")
{
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1i}.sets(2).ways(1).prefetch_buffer_size(0)};
  // Physical and virtual addresses intentionally differ, including the set index.
  auto& resident = cache.block[1];
  resident.valid = true;
  resident.address = champsim::address{0x2040};
  resident.v_address = champsim::address{0x1000};
  fdip predictor{&cache};
  predictor.prefetcher_branch_operate({}, BRANCH_DIRECT_JUMP, champsim::address{0x100c});
  predictor.prefetcher_branch_operate({}, BRANCH_DIRECT_JUMP, champsim::address{0x3000});
  predictor.prefetcher_cycle_operate();
  REQUIRE(predictor.queued_targets.empty());
  REQUIRE(cache.sim_stats.pf_requested == 1);
  REQUIRE(cache.sim_stats.pf_issued == 1);
  REQUIRE(predictor.recent_prefetches.size() == 1);
  REQUIRE(predictor.recent_prefetches.front() == champsim::block_number{champsim::address{0x3000}});
  REQUIRE(cache.is_instruction_present(champsim::address{0x100c}));
  REQUIRE_FALSE(cache.is_instruction_present(champsim::address{0x2040}));
}

TEST_CASE("FDIP checks UBS target bytes instead of treating a matching tag as a full hit", "[fdip-presence]")
{
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1i}.sets(1).set_ubs().prefetch_buffer_size(0)};
  auto& first = cache.block[0];
  first.valid = true;
  first.address = champsim::address{0x2000};
  first.v_address = champsim::address{0x1000};
  first.offset = 0;
  first.size = 4;
  REQUIRE(cache.is_instruction_present(champsim::address{0x1000}));
  REQUIRE_FALSE(cache.is_instruction_present(champsim::address{0x1004}));
  // Adjacent resident fragments together cover the target instruction.
  auto& second = cache.block[1];
  second = first;
  second.offset = 4;
  second.size = 4;
  REQUIRE(cache.is_instruction_present(champsim::address{0x1002}));
  fdip predictor{&cache};
  predictor.prefetcher_branch_operate({}, BRANCH_DIRECT_JUMP, champsim::address{0x1000});
  predictor.prefetcher_cycle_operate();
  REQUIRE(cache.sim_stats.pf_requested == 0);
  predictor.prefetcher_branch_operate({}, BRANCH_DIRECT_JUMP, champsim::address{0x1020});
  predictor.prefetcher_cycle_operate();
  REQUIRE(cache.sim_stats.pf_requested == 1);
}
