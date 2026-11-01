#include <catch.hpp>

#include "../../../btb/basic_btb/basic_btb.h"
#include "cache.h"
#include "defaults.hpp"
#include "dram_controller.h"
#include "vmem.h"
#include "instr.h"
#include "mocks.hpp"
#include "ooo_cpu.h"

TEST_CASE("Ideal BPU predicts cold branches of every type", "[ideal-frontend]")
{
  for (auto type : {BRANCH_DIRECT_JUMP, BRANCH_INDIRECT, BRANCH_CONDITIONAL, BRANCH_DIRECT_CALL, BRANCH_INDIRECT_CALL, BRANCH_RETURN, BRANCH_OTHER}) {
    for (bool taken : {false, true}) {
      if (!taken && type != BRANCH_CONDITIONAL && type != BRANCH_OTHER)
        continue;
      CACHE l1i{champsim::cache_builder{champsim::defaults::default_l1i}};
      O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.l1i(&l1i).btb<basic_btb>().ideal_cbp(true).ideal_ibp(true).ideal_btb(true)};
      cpu.warmup = false;
      auto instr = champsim::test::instruction_with_ip(0x100);
      instr.is_branch = true;
      instr.branch = type;
      instr.branch_taken = taken;
      instr.branch_target = champsim::address{taken ? 0x200u : 0u};
      cpu.do_predict_branch(instr);
      REQUIRE_FALSE(instr.branch_mispredicted);
      REQUIRE(instr.branch_prediction == taken);
      REQUIRE(cpu.prediction_oracle == nullptr);
    }
  }
}

TEST_CASE("Ideal IBP preserves BTB misses and corrects indirect targets only", "[ideal-frontend]")
{
  O3_CPU cpu{champsim::core_builder{}.ideal_ibp(true)};
  basic_btb btb{&cpu};
  auto instr = champsim::test::instruction_with_ip(0x100);
  instr.is_branch = true;
  instr.branch = BRANCH_INDIRECT;
  instr.branch_target = champsim::address{0x300};
  cpu.prediction_oracle = &instr;
  REQUIRE(btb.btb_prediction(instr.ip).first == champsim::address{});
  btb.update_btb(instr.ip, champsim::address{0x200}, true, BRANCH_INDIRECT);
  REQUIRE(btb.btb_prediction(instr.ip).first == instr.branch_target);
  cpu.prediction_oracle = nullptr;
  REQUIRE(btb.btb_prediction(instr.ip).first == champsim::address{0x200});
}

TEST_CASE("Ideal BTB preserves indirect prediction errors", "[ideal-frontend]")
{
  O3_CPU cpu{champsim::core_builder{}.ideal_btb(true)};
  basic_btb btb{&cpu};
  auto instr = champsim::test::instruction_with_ip(0x100);
  instr.is_branch = true;
  instr.branch = BRANCH_INDIRECT;
  instr.branch_target = champsim::address{0x300};
  cpu.prediction_oracle = &instr;
  REQUIRE(btb.btb_prediction(instr.ip).first != instr.branch_target);
  instr.branch = BRANCH_DIRECT_JUMP;
  REQUIRE(btb.btb_prediction(instr.ip).first == instr.branch_target);
}

TEST_CASE("Ideal BTB lookahead retains learned branches beyond finite BTB capacity without a dynamic oracle", "[ideal-frontend][fdip-flow]")
{
  O3_CPU cpu{champsim::core_builder{}.ideal_btb(true).btb_sets(1).btb_ways(1)};
  basic_btb btb{&cpu};
  btb.update_btb(champsim::address{0x100}, champsim::address{0x300}, true, BRANCH_DIRECT_JUMP);
  btb.update_btb(champsim::address{0x200}, champsim::address{0x400}, true, BRANCH_DIRECT_JUMP);
  REQUIRE(cpu.prediction_oracle == nullptr);
  REQUIRE(btb.btb_prediction(champsim::address{0x100}).first == champsim::address{0x300});
  REQUIRE(btb.btb_prediction(champsim::address{0x200}).first == champsim::address{0x400});
  REQUIRE(btb.btb_prediction(champsim::address{0x500}).first == champsim::address{});
}

TEST_CASE("Ideal I-cache returns cold and conflicting lines without lower requests", "[ideal-frontend]")
{
  do_nothing_MRC lower;
  to_rq_MRP upper;
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1i}.ideal(true).sets(1).ways(1)
                  .upper_levels({&upper.queues}).lower_level(&lower.queues).hit_latency(3)};
  std::array<champsim::operable*, 3> elements{{&lower, &cache, &upper}};
  for (auto* element : elements) {
    element->initialize();
    element->warmup = false;
    element->begin_phase();
  }
  for (unsigned i = 0; i < 4; ++i) {
    champsim::channel::request_type request;
    request.address = champsim::address{0x1000 + i * 64};
    request.v_address = request.address;
    request.cpu = 0;
    request.type = access_type::LOAD;
    request.is_translated = true;
    REQUIRE(upper.issue(request));
    for (int cycle = 0; cycle < 20; ++cycle)
      for (auto* element : elements)
        element->_operate();
  }
  REQUIRE(upper.packets.size() == 4);
  for (const auto& packet : upper.packets)
    REQUIRE(packet.return_time - packet.issue_time >= 3);
  REQUIRE(lower.packet_count() == 0);
  REQUIRE(cache.sim_stats.hits.value_or(std::pair{access_type::LOAD, 0u}, 0) == 4);
  REQUIRE(cache.is_instruction_present(champsim::address{0xdeadbeef}));
}

TEST_CASE("Ideal CBP leaves cold target misses intact", "[ideal-frontend]")
{
  CACHE l1i{champsim::cache_builder{champsim::defaults::default_l1i}};
  O3_CPU cpu{champsim::core_builder{champsim::defaults::default_core}.l1i(&l1i).btb<basic_btb>().ideal_cbp(true)};
  cpu.warmup = false;
  auto instr = champsim::test::instruction_with_ip(0x100);
  instr.is_branch = true;
  instr.branch = BRANCH_CONDITIONAL;
  instr.branch_taken = true;
  instr.branch_target = champsim::address{0x200};
  cpu.do_predict_branch(instr);
  REQUIRE(instr.branch_prediction);
  REQUIRE(instr.branch_mispredicted);
}

TEST_CASE("Ideal ITLB returns shared VM mappings without page walks", "[ideal-frontend][ideal-itlb]")
{
  MEMORY_CONTROLLER dram{champsim::chrono::picoseconds{3200}, champsim::chrono::picoseconds{6400},
                         18, 18, 18, 38, champsim::chrono::microseconds{64000}, {},
                         64, 64, 1, champsim::data::bytes{8}, 1024, 1024, 4, 4, 4, 8192};
  VirtualMemory vmem{champsim::data::bytes{4096}, 5, std::chrono::nanoseconds{6400}, dram};
  do_nothing_MRC lower;
  to_rq_MRP upper{[&vmem](auto request, auto response) {
    if (request.address != response.address)
      return false;
    const auto expected = champsim::address{vmem.va_to_pa(request.cpu, champsim::page_number{request.v_address}).first};
    REQUIRE(response.data == expected);
    return true;
  }};
  CACHE cache{champsim::cache_builder{champsim::defaults::default_itlb}.ideal_translation(true, &vmem)
                  .sets(1).ways(1).upper_levels({&upper.queues}).lower_level(&lower.queues).hit_latency(3)};
  std::array<champsim::operable*, 3> elements{{&lower, &cache, &upper}};
  for (auto* element : elements) {
    element->initialize();
    element->warmup = false;
    element->begin_phase();
  }
  // Cold, conflicting and repeated pages must all hit and preserve mappings.
  for (auto address : {0x1000u, 0x2000u, 0x1000u}) {
    champsim::channel::request_type request;
    request.address = champsim::address{address};
    request.v_address = request.address;
    request.cpu = 0;
    request.type = access_type::LOAD;
    request.is_translated = true;
    REQUIRE(upper.issue(request));
    for (int cycle = 0; cycle < 20; ++cycle)
      for (auto* element : elements)
        element->_operate();
    REQUIRE(upper.packets.back().return_time - upper.packets.back().issue_time >= 3);
  }
  REQUIRE(lower.packet_count() == 0);
}
