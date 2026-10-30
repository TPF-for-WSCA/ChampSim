#include <catch.hpp>

#include "instruction.h"
#include "ittage.h"

TEST_CASE("ITTAGE learns context-dependent full-width indirect targets", "[ittage]")
{
  ittage uut;
  const champsim::address ip{0x400100};
  unsigned correct = 0;
  for (unsigned i = 0; i < 8000; ++i) {
    const bool context = (i % 2) != 0;
    const champsim::address target{context ? 0xffff123400008000ULL : 0xabcd567800004000ULL};
    uut.update(champsim::address{0x400080}, champsim::address{0x400090}, context, BRANCH_CONDITIONAL);
    const auto prediction = uut.prediction(ip);
    if (i >= 4000 && prediction.first == target)
      ++correct;
    uut.update(ip, target, true, BRANCH_INDIRECT);
  }
  REQUIRE(correct > 3900);
}

TEST_CASE("ITTAGE has independent state and lookup-only accesses do not train", "[ittage]")
{
  ittage reference, interleaved, other_core;
  const champsim::address ip{0x100000001000};
  REQUIRE(reference.prediction(ip).first == champsim::address{});
  for (unsigned i = 0; i < 6000; ++i) {
    const champsim::address target{0x200000004000ULL + 4 * (i % 3)};
    const bool context = i % 7 == 0;
    reference.update(champsim::address{0x1000}, champsim::address{}, context, BRANCH_CONDITIONAL);
    interleaved.update(champsim::address{0x1000}, champsim::address{}, context, BRANCH_CONDITIONAL);
    REQUIRE(reference.prediction(ip) == interleaved.prediction(ip));
    (void)interleaved.prediction(champsim::address{0xdeadbeef});
    (void)interleaved.prediction(ip);
    other_core.update(ip, champsim::address{0x8000}, true, BRANCH_INDIRECT_CALL);
    reference.update(ip, target, true, BRANCH_INDIRECT_CALL);
    interleaved.update(ip, target, true, BRANCH_INDIRECT_CALL);
  }
  ittage fresh;
  REQUIRE(fresh.prediction(ip).first == champsim::address{});
  REQUIRE(ittage::storage_bits <= 64 * 1024 * 8);
}

TEST_CASE("ITTAGE ignores unknown indirect targets and non-indirect targets for training", "[ittage]")
{
  ittage uut;
  const champsim::address ip{0x400100};
  uut.update(ip, champsim::address{}, true, BRANCH_INDIRECT);
  REQUIRE(uut.prediction(ip).first == champsim::address{});
  for (auto type : {BRANCH_CONDITIONAL, BRANCH_DIRECT_JUMP, BRANCH_DIRECT_CALL, BRANCH_RETURN, BRANCH_OTHER})
    uut.update(ip, champsim::address{0x400200}, true, type);
  REQUIRE(uut.prediction(ip).first == champsim::address{});
}
