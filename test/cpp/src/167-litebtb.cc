#define CATCH_CONFIG_ENABLE_PAIR_STRINGMAKER
#include <catch.hpp>

#include "../../../btb/xbtbx/xbtbx.h"
#include "instruction.h"
#include "ooo_cpu.h"
#include "storage.h"

namespace
{
O3_CPU make_core()
{
  return O3_CPU{champsim::core_builder{}.btb_sets(1).btb_ways(3).btb_clipped_tag(true).btb_tag_size(8).btb_target_sizes({0, 4, 64})};
}
} // namespace

TEST_CASE("LiteBTB full-target tags use the configured width and preserve the default")
{
  const auto sets = GENERATE(256u, 512u, 1024u, 2048u, 4096u);
  const bool clipped = GENERATE(false, true);
  const bool explicit_width = GENERATE(false, true);
  auto builder = champsim::core_builder{}.btb_sets(sets).btb_ways(1).btb_target_sizes({64}).btb_tag_size(13).btb_clipped_tag(clipped);
  if (explicit_width)
    builder.btb_full_target_tag_size(15);
  auto core = O3_CPU{builder};
  xbtbx uut{&core};
  const auto shift = 2 + champsim::storage_bits(sets);
  const champsim::address ip{0x1000}, target{0xffff123400008000ULL};
  uut.update_btb(ip, target, true, BRANCH_DIRECT_JUMP);
  const champsim::address alias{ip.to<uint64_t>() ^ (uint64_t{1} << (shift + 15))};
  CHECK(uut.btb_prediction(alias).first == (clipped && explicit_width ? target : champsim::address{}));
  CHECK(uut.btb_prediction(champsim::address{ip.to<uint64_t>() ^ (uint64_t{1} << (shift + 14))}).first == champsim::address{});
  CHECK(uut.btb_prediction(ip + 4).first == champsim::address{});
}

TEST_CASE("LiteBTB narrow ways retain their tag width when the full-target way uses 15 bits")
{
  auto core = O3_CPU{champsim::core_builder{}.btb_sets(256).btb_ways(2).btb_target_sizes({4, 64})
                        .btb_clipped_tag(true).btb_tag_size(13).btb_full_target_tag_size(15)};
  xbtbx uut{&core};
  const champsim::address ip{0x1000}, near_target{0x1010}, far_target{0xffff123400008000ULL};
  const champsim::address narrow_alias{ip.to<uint64_t>() ^ (uint64_t{1} << (10 + 13))};
  const champsim::address full_alias{ip.to<uint64_t>() ^ (uint64_t{1} << (10 + 15))};
  uut.update_btb(ip, near_target, true, BRANCH_DIRECT_JUMP);
  CHECK(uut.btb_prediction(narrow_alias).first == near_target);
  uut.update_btb(ip, far_target, true, BRANCH_DIRECT_JUMP);
  CHECK(uut.btb_prediction(narrow_alias).first == champsim::address{});
  CHECK(uut.btb_prediction(full_alias).first == far_target);
  const champsim::address updated{0xabcd567800004000ULL};
  uut.update_btb(full_alias, updated, true, BRANCH_CONDITIONAL);
  CHECK(uut.btb_prediction(ip) == std::pair{updated, false});
}

TEST_CASE("LiteBTB rejects invalid full-target tag widths")
{
  auto core = O3_CPU{champsim::core_builder{}.btb_sets(256).btb_ways(1).btb_target_sizes({64}).btb_full_target_tag_size(0)};
  CHECK_THROWS_AS((xbtbx{&core}), std::invalid_argument);
  auto oversized = O3_CPU{champsim::core_builder{}.btb_sets(256).btb_ways(1).btb_target_sizes({64}).btb_full_target_tag_size(55)};
  CHECK_THROWS_AS((xbtbx{&oversized}), std::invalid_argument);
}

TEST_CASE("LiteBTB stores a target in the smallest eligible way")
{
  auto core = make_core();
  xbtbx uut{&core};
  const champsim::address ip{0x1000};
  const champsim::address target{0x1010};

  uut.update_btb(ip, target, true, BRANCH_DIRECT_JUMP);
  CHECK(uut.btb_prediction(ip) == std::pair{target, true});
}

TEST_CASE("LiteBTB migrates a branch when its target outgrows its way")
{
  auto core = make_core();
  xbtbx uut{&core};
  const champsim::address ip{0x1000};
  const champsim::address near_target{0x1010};
  const champsim::address far_target{0x100000};

  uut.update_btb(ip, near_target, true, BRANCH_DIRECT_JUMP);
  uut.update_btb(ip, far_target, true, BRANCH_DIRECT_JUMP);
  CHECK(uut.btb_prediction(ip) == std::pair{far_target, true});
}

TEST_CASE("LiteBTB preserves conditional and return prediction semantics")
{
  auto core = make_core();
  xbtbx uut{&core};
  const champsim::address call_ip{0x2000};
  const champsim::address return_ip{0x3100};

  uut.update_btb(return_ip, champsim::address{0x2004}, true, BRANCH_RETURN);
  uut.update_btb(call_ip, champsim::address{0x4000}, true, BRANCH_DIRECT_CALL);
  CHECK(uut.btb_prediction(return_ip) == std::pair{champsim::address{0x2004}, true});

  const champsim::address conditional_ip{0x5000};
  const champsim::address conditional_target{0x5010};
  uut.update_btb(conditional_ip, conditional_target, true, BRANCH_CONDITIONAL);
  CHECK(uut.btb_prediction(conditional_ip) == std::pair{conditional_target, false});
}
