#include <catch.hpp>

#include <array>
#include <map>
#include <stdexcept>
#include <vector>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"

namespace
{
std::map<CACHE*, std::vector<bool>> ubs_prefetcher_hits;

struct ubs_prefetcher_observer : champsim::modules::prefetcher {
  using prefetcher::prefetcher;

  uint32_t prefetcher_cache_operate(champsim::address, champsim::address, bool cache_hit, bool, access_type, uint32_t metadata)
  {
    ubs_prefetcher_hits[intern_].push_back(cache_hit);
    return metadata;
  }
};

template <typename Range>
void run_cycles(Range& elements, int cycles)
{
  for (int cycle = 0; cycle < cycles; ++cycle)
    for (auto* element : elements)
      element->_operate();
}

auto make_request(champsim::address address, uint64_t bytes, uint64_t id)
{
  champsim::channel::request_type request;
  request.address = address;
  request.v_address = address;
  request.ip = address;
  request.cpu = 0;
  request.instr_id = id;
  request.type = access_type::LOAD;
  request.is_translated = true;
  request.requested_bytes = bytes;
  return request;
}
} // namespace

SCENARIO("UBS predictor evictions install useful sub-blocks and preserve partial-miss bytes")
{
  do_nothing_MRC lower;
  to_rq_MRP upper;
  CACHE uut{champsim::cache_builder{champsim::defaults::default_l1i}
                .name("409-ubs")
                .sets(1)
                .upper_levels({&upper.queues})
                .lower_level(&lower.queues)
                .hit_latency(1)
                .fill_latency(1)
                .set_ubs()
                .ubs_way_sizes({4, 8, 64})
                .ubs_candidate_ways(2)
                .prefetcher<ubs_prefetcher_observer>()};

  std::array<champsim::operable*, 3> elements{{&lower, &uut, &upper}};
  for (auto* element : elements) {
    element->initialize();
    element->warmup = false;
    element->begin_phase();
  }

  const champsim::address line_a{0x1000};
  const champsim::address line_b{0x1040};
  const champsim::address line_c{0x1080};
  uint64_t id = 1;

  REQUIRE(upper.issue(make_request(line_a, 0xfull, id++)));
  run_cycles(elements, 20);
  REQUIRE(upper.issue(make_request(line_b, 0xfull, id++)));
  run_cycles(elements, 20); // evicts A from the predictor into a 4-byte UBS way

  ubs_prefetcher_hits[&uut].clear();
  const auto lower_requests_before_hit = lower.packet_count();
  REQUIRE(upper.issue(make_request(line_a, 0xfull, id++)));
  run_cycles(elements, 10);
  REQUIRE(lower.packet_count() == lower_requests_before_hit);
  REQUIRE(ubs_prefetcher_hits.at(&uut).back());

  const uint64_t second_instruction = uint64_t{0xf} << 8;
  REQUIRE(upper.issue(make_request(line_a + 8, second_instruction, id++)));
  run_cycles(elements, 20);

  REQUIRE(lower.packet_count() == lower_requests_before_hit + 1);
  REQUIRE_FALSE(ubs_prefetcher_hits.at(&uut).back());
  REQUIRE(uut.sim_stats.partial_misses.value_or(std::pair{access_type::LOAD, 0u}, 0) == 1);

  REQUIRE(upper.issue(make_request(line_c, 0xfull, id++)));
  run_cycles(elements, 20); // evicts A again, now retaining both accessed regions

  const auto lower_requests_before_preserved_hits = lower.packet_count();
  REQUIRE(upper.issue(make_request(line_a, 0xfull, id++)));
  REQUIRE(upper.issue(make_request(line_a + 8, second_instruction, id++)));
  run_cycles(elements, 20);
  REQUIRE(lower.packet_count() == lower_requests_before_preserved_hits);

  // A single fetch may depend on instructions held by different sub-blocks.
  // Coverage must be computed from their union, not from one matching way.
  REQUIRE(upper.issue(make_request(line_a, 0xfull | second_instruction, id++)));
  run_cycles(elements, 10);
  REQUIRE(lower.packet_count() == lower_requests_before_preserved_hits);
}

SCENARIO("Disabling UBS retains conventional full-line cache behavior")
{
  do_nothing_MRC lower;
  to_rq_MRP upper;
  CACHE uut{champsim::cache_builder{champsim::defaults::default_l1i}
                .name("409-baseline")
                .sets(1)
                .ways(1)
                .upper_levels({&upper.queues})
                .lower_level(&lower.queues)
                .hit_latency(1)
                .fill_latency(1)
                .reset_ubs()
                .ubs_way_sizes({4, 8})
                .prefetcher<ubs_prefetcher_observer>()};

  std::array<champsim::operable*, 3> elements{{&lower, &uut, &upper}};
  for (auto* element : elements) {
    element->initialize();
    element->warmup = false;
    element->begin_phase();
  }

  REQUIRE_FALSE(uut.ubs);
  REQUIRE(uut.NUM_WAY == 1);

  const champsim::address line{0x2000};
  REQUIRE(upper.issue(make_request(line, 0xfull, 1)));
  run_cycles(elements, 20);

  const auto lower_requests_after_fill = lower.packet_count();
  REQUIRE(upper.issue(make_request(line + 32, uint64_t{0xf} << 32, 2)));
  run_cycles(elements, 10);

  REQUIRE(lower.packet_count() == lower_requests_after_fill);
  REQUIRE(ubs_prefetcher_hits.at(&uut).back());
  REQUIRE(uut.sim_stats.partial_misses.value_or(std::pair{access_type::LOAD, 0u}, 0) == 0);
}

SCENARIO("A partial fill preserves an LRU fragment while all candidate ways are occupied")
{
  do_nothing_MRC lower;
  to_rq_MRP upper;
  CACHE uut{champsim::cache_builder{champsim::defaults::default_l1i}
                .name("409-full-candidates")
                .sets(1)
                .upper_levels({&upper.queues})
                .lower_level(&lower.queues)
                .hit_latency(1)
                .fill_latency(1)
                .set_ubs()
                .ubs_way_sizes({4, 4, 64})
                .ubs_candidate_ways(2)};

  std::array<champsim::operable*, 3> elements{{&lower, &uut, &upper}};
  for (auto* element : elements) {
    element->initialize();
    element->warmup = false;
    element->begin_phase();
  }

  const champsim::address line_a{0x3000};
  const champsim::address line_b{0x3040};
  const champsim::address line_c{0x3080};
  const champsim::address line_d{0x30c0};
  const uint64_t first_instruction = 0xfull;
  const uint64_t second_instruction = uint64_t{0xf} << 8;
  uint64_t id = 1;

  REQUIRE(upper.issue(make_request(line_a, first_instruction, id++)));
  run_cycles(elements, 20);
  REQUIRE(upper.issue(make_request(line_b, first_instruction, id++)));
  run_cycles(elements, 20); // A becomes the first, and therefore LRU, UBS fragment
  REQUIRE(upper.issue(make_request(line_c, first_instruction, id++)));
  run_cycles(elements, 20); // B occupies the second candidate way

  REQUIRE(upper.issue(make_request(line_a + 8, second_instruction, id++)));
  run_cycles(elements, 20); // partial A fill must retain its old LRU fragment
  REQUIRE(uut.sim_stats.partial_misses.value_or(std::pair{access_type::LOAD, 0u}, 0) == 1);

  REQUIRE(upper.issue(make_request(line_d, first_instruction, id++)));
  run_cycles(elements, 20); // evict A's predictor entry back into occupied UBS ways

  const auto lower_requests_before_a = lower.packet_count();
  REQUIRE(upper.issue(make_request(line_a, first_instruction | second_instruction, id++)));
  run_cycles(elements, 10);
  REQUIRE(lower.packet_count() == lower_requests_before_a);
}

TEST_CASE("UBS rejects incompatible cache organizations")
{
  using namespace champsim::data::data_literals;

  REQUIRE_THROWS_AS(CACHE{champsim::cache_builder{}.set_ubs().ubs_way_sizes({})}, std::invalid_argument);
  REQUIRE_THROWS_AS(CACHE{champsim::cache_builder{}.set_ubs().ubs_way_sizes({0, 4})}, std::invalid_argument);
  REQUIRE_THROWS_AS(CACHE{champsim::cache_builder{}.set_ubs().ubs_way_sizes({8, 4})}, std::invalid_argument);
  REQUIRE_THROWS_AS(CACHE{champsim::cache_builder{}.set_ubs().ubs_way_sizes({4, 8})}, std::invalid_argument);
  REQUIRE_THROWS_AS(CACHE{champsim::cache_builder{}.set_ubs().ubs_way_sizes({4, 65})}, std::invalid_argument);
  REQUIRE_THROWS_AS(CACHE{champsim::cache_builder{}.set_ubs().ubs_candidate_ways(0)}, std::invalid_argument);
  REQUIRE_THROWS_AS(CACHE{champsim::cache_builder{}.set_ubs().offset_bits(5_b)}, std::invalid_argument);
}

