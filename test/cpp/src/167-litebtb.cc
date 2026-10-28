#define CATCH_CONFIG_ENABLE_PAIR_STRINGMAKER
#include <catch.hpp>

#include "../../../btb/xbtbx/xbtbx.h"
#include "instruction.h"
#include "ooo_cpu.h"

namespace
{
O3_CPU make_core()
{
  return O3_CPU{champsim::core_builder{}.btb_sets(1).btb_ways(3).btb_clipped_tag(true).btb_tag_size(8).btb_target_sizes({0, 4, 64})};
}
} // namespace

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
