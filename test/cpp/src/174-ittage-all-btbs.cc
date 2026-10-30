#include <catch.hpp>

#include "instruction.h"
#include "ooo_cpu.h"
#include "../../../btb/basic_btb/basic_btb.h"
#include "../../../btb/fullxor_hash_xbtbx/fullxor_hash_xbtbx.h"
#include "../../../btb/geometric_xbtbx/geometric_xbtbx.h"
#include "../../../btb/hash_xbtbx/hash_xbtbx.h"
#include "../../../btb/pearson_xbtbx/pearson_xbtbx.h"
#include "../../../btb/rabin_xbtbx/rabin_xbtbx.h"
#include "../../../btb/sha3_xbtbx/sha3_xbtbx.h"
#include "../../../btb/two_level_full_hashed_xbtbx/two_level_full_hashed_xbtbx.h"
#include "../../../btb/two_level_geometric_hashed_xbtbx/two_level_geometric_hashed_xbtbx.h"
#include "../../../btb/two_level_hashed_xbtbx/two_level_hashed_xbtbx.h"
#include "../../../btb/two_level_xbtbx/two_level_xbtbx.h"
#include "../../../btb/xbtbx/xbtbx.h"
#include "../../../btb/zobrist_xbtbx/zobrist_xbtbx.h"

TEMPLATE_TEST_CASE("All BTB modules use ITTAGE for context-dependent indirect targets", "[ittage][btb]",
                   basic_btb, fullxor_hash_xbtbx, geometric_xbtbx, hash_xbtbx, pearson_xbtbx, rabin_xbtbx, sha3_xbtbx, two_level_full_hashed_xbtbx, two_level_geometric_hashed_xbtbx, two_level_hashed_xbtbx, two_level_xbtbx, xbtbx, zobrist_xbtbx)
{
  auto core = O3_CPU{champsim::core_builder{}.btb_sets(64).btb_ways(8).btb_clipped_tag(false).btb_target_sizes({64, 64, 64, 64, 64, 64, 64, 64})};
  TestType uut{&core};
  const champsim::address ip{0x400100};
  for (auto type : {BRANCH_INDIRECT, BRANCH_INDIRECT_CALL}) {
    unsigned correct = 0;
    for (unsigned i = 0; i < 6000; ++i) {
      const bool context = i % 2 != 0;
      const champsim::address target{context ? 0xffff123400008000ULL : 0xabcd567800004000ULL};
      uut.update_btb(champsim::address{0x400080}, champsim::address{0x400090}, context, BRANCH_CONDITIONAL);
      const auto prediction = uut.btb_prediction(ip);
      if (i >= 3000 && prediction.first == target && prediction.second)
        ++correct;
      uut.update_btb(ip, target, true, type);
    }
    // Last-target prediction fails on every alternating target. This checks the
    // actual module dispatch, including two-level BTB hits, rather than just fill.
    REQUIRE(correct > 2900);
  }
}
