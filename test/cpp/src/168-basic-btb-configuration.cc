#include <catch.hpp>

#include "../../../btb/basic_btb/basic_btb.h"
#include "../../../prefetcher/fdip/fdip.h"
#include "cache.h"
#include "instruction.h"
#include "ooo_cpu.h"
#include "storage.h"

TEST_CASE("Basic BTB defaults retain full tags")
{
  const auto sets = GENERATE(256u, 512u, 1024u, 2048u, 4096u);
  direct_predictor uut{sets, 8};
  CHECK(uut.tag_bits == 62 - champsim::storage_bits(sets));
  const champsim::address ip{0x1000};
  uut.update(ip, champsim::address{0x2000}, BRANCH_DIRECT_JUMP);
  const champsim::address alias{ip.to<uint64_t>() + (uint64_t{1} << (2 + champsim::storage_bits(sets) + 15))};
  CHECK_FALSE(uut.check_hit(alias).has_value());
}

TEST_CASE("Basic BTB configured tags alias only above the index and retained tag")
{
  const auto sets = GENERATE(256u, 512u, 1024u, 2048u, 4096u);
  direct_predictor uut{sets, 8, 15};
  CHECK(uut.tag_bits == 15);
  const auto shift = 2 + champsim::storage_bits(sets);
  const champsim::address ip{0x1000};
  const champsim::address target{0x2000};
  const champsim::address alias{ip.to<uint64_t>() + (uint64_t{1} << (shift + 15))};
  uut.update(ip, target, BRANCH_DIRECT_JUMP);
  REQUIRE(uut.check_hit(alias).has_value());
  CHECK(uut.check_hit(alias)->target == target);
  CHECK_FALSE(uut.check_hit(ip + 4).has_value()); // A different set.
  CHECK_FALSE(uut.check_hit(champsim::address{ip.to<uint64_t>() ^ (uint64_t{1} << (shift + 14))}).has_value());
  // Training an alias updates the same hardware entry, including its type.
  uut.update(alias, champsim::address{0x3000}, BRANCH_CONDITIONAL);
  REQUIRE(uut.check_hit(ip).has_value());
  CHECK(uut.check_hit(ip)->target == champsim::address{0x3000});
  CHECK(uut.check_hit(ip)->type == direct_predictor::branch_info::CONDITIONAL);
}

TEST_CASE("Basic BTB receives configured tag clipping from the core")
{
  const bool clipped = GENERATE(false, true);
  const bool explicit_tag = GENERATE(false, true);
  auto builder = champsim::core_builder{}.btb_sets(256).btb_ways(8).btb_clipped_tag(clipped);
  if (explicit_tag)
    builder.btb_tag_size(15);
  auto core = O3_CPU{builder};
  basic_btb uut{&core};
  uut.update_btb(champsim::address{0x1000}, champsim::address{0x2000}, true, BRANCH_DIRECT_JUMP);
  const auto prediction = uut.btb_prediction(champsim::address{0x1000 + (uint64_t{1} << 25)});
  CHECK(prediction.first == (clipped && explicit_tag ? champsim::address{0x2000} : champsim::address{}));
}

TEST_CASE("Basic BTB rejects invalid tag widths")
{
  CHECK_THROWS_AS((direct_predictor{256, 8, 0}), std::invalid_argument);
  CHECK_THROWS_AS((direct_predictor{256, 8, 55}), std::invalid_argument);
}

TEST_CASE("Basic BTB wrong-path hits preserve the LRU victim")
{
  const bool wrong_path = GENERATE(false, true);
  auto core = O3_CPU{champsim::core_builder{}.btb_sets(1).btb_ways(2)};
  basic_btb uut{&core};
  const champsim::address oldest{0x1000}, newer{0x1004}, incoming{0x1008};
  uut.update_btb(oldest, champsim::address{0x2000}, true, BRANCH_DIRECT_JUMP);
  uut.update_btb(newer, champsim::address{0x2004}, true, BRANCH_DIRECT_JUMP);

  core.wrong_path_active = wrong_path;
  CHECK(uut.btb_prediction(oldest).first == champsim::address{0x2000});
  core.wrong_path_active = false;
  uut.update_btb(incoming, champsim::address{0x2008}, true, BRANCH_DIRECT_JUMP);

  CHECK(uut.btb_prediction(wrong_path ? oldest : newer).first == champsim::address{});
  CHECK(uut.btb_prediction(wrong_path ? newer : oldest).first != champsim::address{});
}

TEST_CASE("Basic BTB allocation respects configured sets and ways")
{
  const auto ways = GENERATE(2u, 4u);
  auto core = O3_CPU{champsim::core_builder{}.btb_sets(1).btb_ways(ways)};
  basic_btb uut{&core};
  for (unsigned i = 0; i < ways; ++i)
    uut.update_btb(champsim::address{0x1000 + i * 4}, champsim::address{0x2000 + i * 4}, true, BRANCH_DIRECT_JUMP);
  CHECK(uut.btb_prediction(champsim::address{0x1000}).first == champsim::address{0x2000});
  // Touching the oldest entry makes the second entry the LRU victim.
  uut.update_btb(champsim::address{0x1000 + ways * 4}, champsim::address{0x3000}, true, BRANCH_DIRECT_JUMP);
  CHECK(uut.btb_prediction(champsim::address{0x1004}).first == champsim::address{});
  CHECK(uut.btb_prediction(champsim::address{0x1000}).first == champsim::address{0x2000});
}

TEST_CASE("Basic BTB recovery lookups preserve replacement state with replay disabled")
{
  const bool replay = GENERATE(false, true);
  auto core = O3_CPU{champsim::core_builder{}.btb_sets(1).btb_ways(2).wrong_path(replay)};
  basic_btb uut{&core};
  const champsim::address oldest{0x1000}, newer{0x1004}, incoming{0x1008};
  uut.update_btb(oldest, champsim::address{0x2000}, true, BRANCH_DIRECT_JUMP);
  uut.update_btb(newer, champsim::address{0x2004}, true, BRANCH_DIRECT_JUMP);
  core.branch_recovery_start = core.current_time;
  core.wrong_path_active = replay;
  REQUIRE(uut.btb_prediction(oldest).first == champsim::address{0x2000});
  core.branch_recovery_start.reset();
  core.wrong_path_active = false;
  uut.update_btb(incoming, champsim::address{0x2008}, true, BRANCH_DIRECT_JUMP);
  REQUIRE(uut.btb_prediction(oldest).first == champsim::address{});
  REQUIRE(uut.btb_prediction(newer).first == champsim::address{0x2004});
}

TEST_CASE("Basic BTB recovery preserves speculative return history with either replay policy", "[wrong-path][fdip-flow]")
{
  const bool replay = GENERATE(false, true);
  CACHE cache{champsim::cache_builder{}.prefetcher<fdip>()};
  auto core = O3_CPU{champsim::core_builder{}.l1i(&cache).wrong_path(replay)};
  basic_btb uut{&core};
  const champsim::address outer_call{0x1000}, inner_call{0x2000}, return_pc{0x3000};
  // Install a return entry before the calls so outcome training leaves two
  // outstanding architectural calls to seed the lookahead checkpoint.
  uut.update_btb(return_pc, outer_call + 4, true, BRANCH_RETURN);
  uut.update_btb(outer_call, champsim::address{0x4000}, true, BRANCH_DIRECT_CALL);
  uut.update_btb(inner_call, champsim::address{0x5000}, true, BRANCH_DIRECT_CALL);
  (void)uut.btb_prediction(champsim::address{0x6000});

  core.branch_recovery_start = core.current_time;
  core.wrong_path_active = replay;
  ++core.sim_stats.fdip_fetch_blocks;
  REQUIRE(uut.btb_prediction(return_pc).first == inner_call + 4);
  // Lookahead must retain its pop across blocks even if demand/replay has
  // consumed the last FTQ entry while the older branch remains unresolved.
  REQUIRE(core.fetch_target_queue.empty());
  ++core.sim_stats.fdip_fetch_blocks;
  REQUIRE(uut.btb_prediction(return_pc).first == outer_call + 4);

  core.branch_recovery_start.reset();
  core.wrong_path_active = false;
  ++core.sim_stats.fdip_fetch_blocks;
  // A resolved redirect restarts from architectural history in both modes.
  REQUIRE(uut.btb_prediction(return_pc).first == inner_call + 4);
}
