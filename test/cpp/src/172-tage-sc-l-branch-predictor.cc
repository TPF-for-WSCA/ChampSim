#include <catch.hpp>

#include "../../../branch/tage_sc_l/tage_sc_l.h"
#include "instruction.h"

TEST_CASE("TAGE-SC-L learns constant conditional branches", "[tage_sc_l]")
{
  for (bool taken : {false, true}) {
    tage_sc_l uut{nullptr};
    const champsim::address ip{0x400120};
    unsigned correct = 0;
    for (unsigned i = 0; i < 4096; ++i) {
      const bool prediction = uut.predict_branch(ip);
      if (i >= 3072 && prediction == taken)
        ++correct;
      uut.last_branch_result(ip, champsim::address{0x400100}, taken, BRANCH_CONDITIONAL);
    }
    REQUIRE(correct > 1000);
  }
}

TEST_CASE("TAGE-SC-L lookup scratch and other cores cannot corrupt training", "[tage_sc_l]")
{
  tage_sc_l reference{nullptr};
  tage_sc_l interleaved{nullptr};
  tage_sc_l other_core{nullptr};
  unsigned correct = 0;
  for (unsigned i = 0; i < 20000; ++i) {
    const champsim::address ip{0x400100 + 4 * (i % 7)};
    const champsim::address target{0x400080};
    const bool taken = (i % 13) != 12;
    REQUIRE(reference.predict_branch(ip) == interleaved.predict_branch(ip));
    // Extra lookups model nonbranches and replayed wrong-path branches.
    interleaved.predict_branch(champsim::address{0x500000 + 4 * (i % 31)});
    interleaved.predict_branch(champsim::address{0x600000 + 4 * (i % 17)});
    other_core.predict_branch(ip);
    other_core.last_branch_result(ip, target, !taken, BRANCH_CONDITIONAL);
    const bool prediction = reference.predict_branch(ip);
    if (i >= 10000 && prediction == taken)
      ++correct;
    reference.last_branch_result(ip, target, taken, BRANCH_CONDITIONAL);
    interleaved.last_branch_result(ip, target, taken, BRANCH_CONDITIONAL);
    if (i % 5 == 0) {
      reference.last_branch_result(champsim::address{0x400200}, target, true, BRANCH_DIRECT_CALL);
      interleaved.last_branch_result(champsim::address{0x400200}, target, true, BRANCH_DIRECT_CALL);
      reference.last_branch_result(champsim::address{0x400220}, target, true, BRANCH_RETURN);
      interleaved.last_branch_result(champsim::address{0x400220}, target, true, BRANCH_RETURN);
    }
  }
  REQUIRE(correct > 9000);
}

TEST_CASE("TAGE-SC-L retains trained state when moved", "[tage_sc_l]")
{
  tage_sc_l original{nullptr};
  const champsim::address ip{0x400100};
  for (unsigned i = 0; i < 4096; ++i) {
    original.predict_branch(ip);
    original.last_branch_result(ip, champsim::address{0x400080}, true, BRANCH_CONDITIONAL);
  }
  tage_sc_l moved{std::move(original)};
  REQUIRE(moved.predict_branch(ip));
  tage_sc_l assigned{nullptr};
  assigned = std::move(moved);
  REQUIRE(assigned.predict_branch(ip));
}
