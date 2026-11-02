#include <catch.hpp>

#include "../../../branch/bimodal/bimodal.h"
#include "../../../branch/gshare/gshare.h"
#include "../../../branch/hashed_perceptron/hashed_perceptron.h"
#include "../../../branch/perceptron/perceptron.h"
#include "../../../branch/tage_sc_l/tage_sc_l.h"
#include "../../../prefetcher/fdip/fdip.h"
#include "cache.h"
#include "defaults.hpp"
#include "instr.h"
#include "ooo_cpu.h"

TEMPLATE_TEST_CASE("Prediction-only lookups cannot change correct-path learning", "[wrong-path][branch-predictors]", bimodal, gshare,
                   hashed_perceptron, perceptron, tage_sc_l)
{
  TestType reference{nullptr};
  TestType replayed{nullptr};
  unsigned correct = 0;
  for (unsigned i = 0; i < 4096; ++i) {
    const champsim::address ip{0x400100 + 4 * (i % 7)}, target{0x400080};
    const bool taken = i % 13 != 12;
    const bool prediction = reference.predict_branch(ip);
    REQUIRE(replayed.predict_branch(ip) == prediction);
    // Include repeated PCs and more lookups than the old perceptron update
    // buffer could hold. Neither nonbranches nor squashed branches resolve.
    const unsigned lookups = i % 17 == 0 ? 128 : 3;
    for (unsigned lookup = 0; lookup < lookups; ++lookup)
      replayed.predict_branch(champsim::address{0x500000 + 4 * (lookup % 31)});
    replayed.predict_branch(ip);
    replayed.predict_branch(champsim::address{0x600000});
    if (i >= 3072 && prediction == taken)
      ++correct;
    reference.last_branch_result(ip, target, taken, BRANCH_CONDITIONAL);
    replayed.last_branch_result(ip, target, taken, BRANCH_CONDITIONAL);
  }
  REQUIRE(correct > 900);
}

TEMPLATE_TEST_CASE("Wrong-path recovery preserves trained branch predictions", "[wrong-path][branch-predictors]", bimodal, gshare,
                   hashed_perceptron, perceptron, tage_sc_l)
{
  const bool use_fdip = GENERATE(false, true);
  CACHE plain_cache{champsim::cache_builder{champsim::defaults::default_l1i}};
  CACHE fdip_cache{champsim::cache_builder{champsim::defaults::default_l1i}.prefetcher<fdip>()};
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.l1i(use_fdip ? &fdip_cache : &plain_cache)
                 .wrong_path(true).branch_predictor<TestType>()};
  TestType reference{nullptr};
  cpu.warmup = false;

  // Train an architectural branch. The replayed branch at another PC must
  // never train on its recorded outcome, even when it is fetched repeatedly.
  const champsim::address ip{0x100}, target{0x80};
  for (unsigned i = 0; i < 2048; ++i) {
    reference.predict_branch(ip);
    reference.last_branch_result(ip, target, true, BRANCH_CONDITIONAL);
    cpu.impl_predict_branch(ip, target, false, BRANCH_CONDITIONAL);
    cpu.impl_last_branch_result(ip, target, true, BRANCH_CONDITIONAL);
  }
  auto replay = champsim::test::instruction_with_ip(0x200);
  replay.is_branch = true;
  replay.branch = BRANCH_CONDITIONAL;
  replay.branch_taken = false;
  cpu.remember_instruction(replay);
  for (unsigned recovery = 0; recovery < 32; ++recovery) {
    cpu.wrong_path_active = true;
    cpu.wrong_path_next = replay.ip;
    if (use_fdip)
      cpu.redirect_fdip(replay.ip);
    for (unsigned cycle = 0; cycle < 8; ++cycle)
      cpu.initialize_instruction();
    REQUIRE_FALSE(cpu.IFETCH_BUFFER.empty());
    cpu.recover_wrong_path();
    if (use_fdip)
      cpu.redirect_fdip(ip);
    REQUIRE(cpu.IFETCH_BUFFER.empty());
    REQUIRE(cpu.sim_stats.branch_type_misses.value_or(BRANCH_CONDITIONAL, 0) == 0);
    REQUIRE(cpu.impl_predict_branch(ip, target, false, BRANCH_CONDITIONAL) == reference.predict_branch(ip));
    reference.last_branch_result(ip, target, true, BRANCH_CONDITIONAL);
    cpu.impl_last_branch_result(ip, target, true, BRANCH_CONDITIONAL);
  }
  REQUIRE(cpu.sim_stats.wrong_path_fetched > 0);
  REQUIRE(cpu.sim_stats.wrong_path_squashed == cpu.sim_stats.wrong_path_fetched);
}
