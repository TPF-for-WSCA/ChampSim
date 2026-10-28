#include <catch.hpp>

#include "../../../btb/basic_btb/basic_btb.h"
#include "instruction.h"
#include "ooo_cpu.h"

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
