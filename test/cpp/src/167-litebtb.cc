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

namespace
{
O3_CPU make_utb_core(const std::string& policy, std::size_t entries = 2, std::size_t ways = 2)
{
  return O3_CPU{champsim::core_builder{}.btb_sets(4).btb_ways(1).btb_target_sizes({25}).btb_tag_size(8)
                    .btb_utb_entries(entries).btb_utb_ways(ways).btb_utb_replacement(policy)};
}
void insert(xbtbx& predictor, uint64_t ip, uint8_t type = BRANCH_DIRECT_JUMP)
{
  predictor.update_btb(champsim::address{ip}, champsim::address{ip + 4}, true, type);
}
} // namespace

TEST_CASE("LiteBTB UTB reference counts track BTB entries rather than accesses")
{
  auto core = make_utb_core(GENERATE(std::string{"lru"}, std::string{"reference_count"}));
  xbtbx uut{&core};
  insert(uut, 16); // tag 1, set 0
  CHECK(uut.utb_reference_counts() == std::vector<std::size_t>{1, 0});
  insert(uut, 24); // tag 1, set 2
  CHECK(uut.utb_reference_counts() == std::vector<std::size_t>{2, 0});
  for (int i = 0; i < 5; ++i) {
    insert(uut, 16); // same-tag replacement: decrement and increment cancel
    CHECK(uut.btb_prediction(champsim::address{16}).first == champsim::address{20});
  }
  CHECK(uut.utb_reference_counts() == std::vector<std::size_t>{2, 0});
  insert(uut, 36); // tag 2, set 1
  insert(uut, 32); // tag 2 replaces tag 1 in BTB set 0
  CHECK(uut.utb_reference_counts() == std::vector<std::size_t>{1, 2});
}

TEST_CASE("LiteBTB UTB policies choose LRU or the least referenced tag")
{
  const auto policy = GENERATE(std::string{"lru"}, std::string{"reference_count"});
  auto core = make_utb_core(policy);
  xbtbx uut{&core};
  insert(uut, 16);
  insert(uut, 24); // tag 1 has two references, but is oldest
  insert(uut, 36); // tag 2 has one reference
  insert(uut, 60); // tag 3, set 3: force UTB eviction
  if (policy == "reference_count") {
    CHECK(uut.utb_reference_counts() == std::vector<std::size_t>{2, 1});
    CHECK(uut.btb_prediction(champsim::address{16}).first == champsim::address{20});
    CHECK(uut.btb_prediction(champsim::address{36}).first == champsim::address{});
  } else {
    CHECK(uut.utb_reference_counts() == std::vector<std::size_t>{1, 1});
    CHECK(uut.btb_prediction(champsim::address{16}).first == champsim::address{});
    CHECK(uut.btb_prediction(champsim::address{36}).first == champsim::address{40});
  }
}

TEST_CASE("LiteBTB UTB eviction starts at one and stale pointer removal does not decrement the new tag")
{
  const auto policy = GENERATE(std::string{"lru"}, std::string{"reference_count"});
  auto core = make_utb_core(policy, 1, 1);
  xbtbx uut{&core};
  insert(uut, 16);
  insert(uut, 24);
  REQUIRE(uut.utb_reference_counts() == std::vector<std::size_t>{2});
  insert(uut, 36); // replaces UTB tag 1 with tag 2, retaining old BTB pointers
  REQUIRE(uut.utb_reference_counts() == std::vector<std::size_t>{1});
  REQUIRE(uut.btb_prediction(champsim::address{32}).first != champsim::address{}); // stale pointer aliases tag 2
  REQUIRE(core.prediction_btb_token);
  const auto stale = *core.prediction_btb_token;
  uut.btb_decode(stale, BRANCH_CONDITIONAL); // type mismatch invalidates only that stale BTB entry
  CHECK(uut.btb_prediction(champsim::address{32}).first == champsim::address{});
  CHECK(uut.utb_reference_counts() == std::vector<std::size_t>{1});
  insert(uut, 32);
  CHECK(uut.utb_reference_counts() == std::vector<std::size_t>{2});
  uut.btb_decode(stale, NOT_BRANCH); // old decode cannot invalidate a newer occupant
  CHECK(uut.btb_prediction(champsim::address{32}).first == champsim::address{36});
  REQUIRE(core.prediction_btb_token);
  uut.btb_decode(*core.prediction_btb_token, NOT_BRANCH);
  CHECK(uut.utb_reference_counts() == std::vector<std::size_t>{1});
  CHECK(uut.btb_prediction(champsim::address{32}).first == champsim::address{});
}

TEST_CASE("LiteBTB retained UTB pointers become useful again on tag reinsertion")
{
  auto core = make_utb_core("reference_count", 1, 1);
  xbtbx uut{&core};
  insert(uut, 16);
  insert(uut, 36);
  CHECK(uut.btb_prediction(champsim::address{16}).first == champsim::address{});
  insert(uut, 24); // reinsert tag 1 in another BTB set
  CHECK(uut.utb_reference_counts() == std::vector<std::size_t>{1});
  CHECK(uut.btb_prediction(champsim::address{16}).first == champsim::address{20});
  insert(uut, 16); // adopt the revived pointer into the current generation
  CHECK(uut.utb_reference_counts() == std::vector<std::size_t>{2});
}

TEST_CASE("LiteBTB UTB counter follows target-width migrations and full-target bypass")
{
  auto core = O3_CPU{champsim::core_builder{}.btb_sets(4).btb_ways(3).btb_target_sizes({4, 25, 64}).btb_tag_size(8)
                        .btb_utb_entries(1).btb_utb_ways(1).btb_utb_replacement("reference_count")};
  xbtbx uut{&core};
  insert(uut, 16);
  REQUIRE(uut.utb_reference_counts() == std::vector<std::size_t>{1});
  uut.update_btb(champsim::address{16}, champsim::address{0x100000}, true, BRANCH_DIRECT_JUMP);
  CHECK(uut.utb_reference_counts() == std::vector<std::size_t>{1});
  CHECK(uut.btb_prediction(champsim::address{16}).first == champsim::address{0x100000});
  uut.update_btb(champsim::address{16}, champsim::address{uint64_t{1} << 40}, true, BRANCH_DIRECT_JUMP);
  CHECK(uut.utb_reference_counts() == std::vector<std::size_t>{0});
  CHECK(uut.btb_prediction(champsim::address{16}).first == champsim::address{uint64_t{1} << 40});
}

TEST_CASE("LiteBTB UTB validates geometry and policy")
{
  const auto construct = [](const std::string& policy, std::size_t entries, std::size_t ways) {
    auto core = make_utb_core(policy, entries, ways);
    return xbtbx{&core};
  };
  CHECK_THROWS_AS(construct("lfu", 2, 2), std::invalid_argument);
  CHECK_THROWS_AS(construct("lru", 2, 0), std::invalid_argument);
  CHECK_THROWS_AS(construct("lru", 3, 2), std::invalid_argument);
  CHECK_THROWS_AS(construct("lru", 6, 2), std::invalid_argument);
  CHECK_THROWS_AS(construct("lru", 0, 2), std::invalid_argument);
}

TEST_CASE("LiteBTB UTB replacement is confined to its indexed set")
{
  auto core = make_utb_core("reference_count", 4, 2);
  xbtbx uut{&core};
  insert(uut, 16); // odd tag -> UTB set 1
  insert(uut, 36); // even tag -> UTB set 0
  insert(uut, 56); // odd tag -> UTB set 1
  REQUIRE(uut.utb_reference_counts() == std::vector<std::size_t>{1, 0, 1, 1});
  insert(uut, 92); // odd tag 5 -> evicts the older odd tag, despite free space in set 0
  CHECK(uut.utb_reference_counts() == std::vector<std::size_t>{1, 0, 1, 1});
  CHECK(uut.btb_prediction(champsim::address{16}).first == champsim::address{});
  CHECK(uut.btb_prediction(champsim::address{36}).first == champsim::address{40});
}

TEST_CASE("LiteBTB core carries alias scrubbing tokens through fetch and both decode paths")
{
  const bool dib_hit = GENERATE(false, true);
  auto core = O3_CPU{champsim::core_builder{}.btb<xbtbx>().btb_sets(4).btb_ways(1).btb_target_sizes({25}).btb_tag_size(8)
                        .btb_utb_entries(1).btb_utb_ways(1).btb_utb_replacement("reference_count")};
  auto& uut = std::get<0>(static_cast<O3_CPU::btb_module_model<xbtbx>*>(core.btb_module_pimpl.get())->intern_);
  insert(uut, 16);
  insert(uut, 36);
  ooo_model_instr instruction{0, input_instr{}};
  instruction.ip = champsim::address{32};
  instruction.branch = NOT_BRANCH;
  // Emulate the token preserved by an FDIP prediction in the FTQ.
  const auto prediction = uut.btb_prediction(instruction.ip);
  core.current_fetch_prediction = O3_CPU::fetch_prediction{instruction.ip, prediction.first, prediction.second, true, true, core.prediction_btb_token};
  core.do_predict_branch(instruction);
  REQUIRE(instruction.btb_token);
  REQUIRE(uut.btb_prediction(instruction.ip).first != champsim::address{});
  (dib_hit ? core.DIB_HIT_BUFFER : core.DECODE_BUFFER).push_back(instruction);
  REQUIRE(core.decode_instruction() == 1);
  CHECK(uut.btb_prediction(instruction.ip).first == champsim::address{});
  CHECK(uut.utb_reference_counts() == std::vector<std::size_t>{1});
}
