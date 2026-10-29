#include <catch.hpp>

#include <array>
#include <map>
#include <stdexcept>
#include <vector>

#include "cache.h"
#include "defaults.hpp"
#include "instr.h"
#include "mocks.hpp"
#include "ooo_cpu.h"

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

namespace
{
const std::vector<uint32_t> sweep_layout{4, 4, 8, 8, 8, 12, 12, 16, 24, 32, 36, 36, 52, 64, 64, 64};

auto probe_builder(bool uneven)
{
  auto builder = champsim::cache_builder{champsim::defaults::default_l1i};
  if (uneven)
    builder.set_ubs();
  else
    builder.reset_ubs();
  return builder;
}

struct ubs_probe {
  do_nothing_MRC lower;
  to_rq_MRP upper;
  CACHE cache;
  std::array<champsim::operable*, 3> elements;
  uint64_t id = 1;

  explicit ubs_probe(bool uneven = true, uint32_t candidates = 4, uint32_t ways = 8, int lower_latency = 10)
      : lower(lower_latency), cache(probe_builder(uneven)
                  .name("409-probe")
                  .sets(1)
                  .ways(ways)
                  .upper_levels({&upper.queues})
                  .lower_level(&lower.queues)
                  .hit_latency(1)
                  .fill_latency(1)
                  .ubs_way_sizes(sweep_layout)
                  .ubs_candidate_ways(candidates)),
        elements{{&lower, &cache, &upper}}
  {
    for (auto* element : elements) {
      element->initialize();
      element->warmup = false;
      element->begin_phase();
    }
  }

  void issue(uint64_t line, uint64_t mask)
  {
    REQUIRE(upper.issue(make_request(champsim::address{0x10000 + 64 * line}, mask, id++)));
  }
  void access(uint64_t line, uint64_t mask)
  {
    issue(line, mask);
    run_cycles(elements, 40);
    REQUIRE(upper.packets.back().return_time > 0);
  }
};

uint64_t low_bytes(uint32_t size)
{
  return size == 64 ? ~uint64_t{0} : (uint64_t{1} << size) - 1;
}
} // namespace

TEST_CASE("UBS historical bounds use size transitions and search all fitting empty ways")
{
  const auto size = GENERATE(4u, 8u, 12u, 16u, 24u, 32u, 36u, 52u, 64u);
  ubs_probe probe;
  const auto first = static_cast<std::size_t>(std::find_if(sweep_layout.begin(), sweep_layout.end(),
                                                        [size](auto capacity) { return capacity >= size; }) - sweep_layout.begin());
  for (uint64_t line = 0; line <= 16 - first; ++line)
    probe.access(line, low_bytes(size));
  REQUIRE(static_cast<std::size_t>(std::count_if(probe.cache.block.begin(), probe.cache.block.end(), [](const auto& b) { return b.valid; })) == 16 - first);

  // Golden saturated windows from the historical source-extracted diagnostic.
  const std::map<unsigned, unsigned> ends{{4,9},{8,10},{12,11},{16,13},{24,14},{32,16},{36,16},{52,16},{64,16}};
  const auto end = ends.at(size);
  for (std::size_t target = first; target < 16; ++target) {
    for (auto& block : probe.cache.block)
      block.last_used = 0;
    probe.cache.block.at(target).last_used = 100;
    const auto before = probe.cache.block;
    probe.access(100 + target, low_bytes(size));
    const auto selected = target < end ? target : first;
    REQUIRE(probe.cache.block.at(selected).address != before.at(selected).address);
    REQUIRE(probe.cache.block.at(selected).last_used == 14);
  }
}

TEST_CASE("UBS historical insertion and hit promotion update the original rank subsets")
{
  ubs_probe probe;
  // Prime the predictor; the next fill will insert it into the empty way 0.
  probe.access(0, 0xf);
  for (std::size_t way = 0; way < 16; ++way)
    probe.cache.block.at(way).last_used = 13;
  probe.access(1, 0xf);
  REQUIRE(probe.cache.block.at(0).last_used == 14);
  for (std::size_t way = 1; way < 16; ++way)
    REQUIRE(probe.cache.block.at(way).last_used == (way < 4 ? 14 : 13));
  probe.access(0, 0xf);
  REQUIRE(probe.cache.block.at(0).last_used == 0);
  for (std::size_t way = 1; way < 16; ++way)
    REQUIRE(probe.cache.block.at(way).last_used == (way < 4 ? 15 : 14));
}

TEST_CASE("UBS mixed region sizes can populate every physical way")
{
  ubs_probe probe;
  uint64_t line = 0;
  for (const auto size : {4u, 8u, 12u, 16u, 24u, 32u, 36u, 52u, 64u})
    for (unsigned repeat = 0; repeat < 4; ++repeat)
      probe.access(line++, low_bytes(size));
  probe.access(line, 0xf); // drain final predictor victim
  REQUIRE(std::count_if(probe.cache.block.begin(), probe.cache.block.end(), [](const auto& b) { return b.valid; }) == 16);
}

TEST_CASE("UBS historical empty-slot search retains fitting cyclic working sets")
{
  const auto size = GENERATE(4u, 8u, 64u);
  const auto uneven = GENERATE(false, true);
  const auto conventional_ways = GENERATE(8u, 16u);
  ubs_probe probe{uneven, 4, conventional_ways};
  const unsigned capacity = uneven ? static_cast<unsigned>(std::count_if(sweep_layout.begin(), sweep_layout.end(),
                                                                        [size](auto bytes) { return bytes >= size; })) + 1 : conventional_ways;
  for (unsigned line = 0; line < capacity; ++line)
    probe.access(line, low_bytes(size));
  const auto before = probe.lower.packet_count();
  for (unsigned line = 0; line < capacity; ++line)
    probe.access(line, low_bytes(size));
  REQUIRE(probe.lower.packet_count() == before);
}

TEST_CASE("UBS merged misses and predictor hits retain all requested byte regions")
{
  ubs_probe probe;
  probe.issue(0, 0xf);
  run_cycles(probe.elements, 4);
  probe.issue(0, uint64_t{0xf} << 8);
  run_cycles(probe.elements, 40);
  REQUIRE(probe.lower.packet_count() == 1);
  // This is a predictor hit: its bytes must survive the later eviction too.
  probe.access(0, uint64_t{0xf} << 60);
  REQUIRE(probe.lower.packet_count() == 1);
  probe.access(1, 0xf);
  uint64_t present = 0;
  for (const auto& block : probe.cache.block)
    if (block.valid && block.address == champsim::address{0x10000})
      present |= low_bytes(block.size) << block.offset;
  const auto requested = 0xfull | (uint64_t{0xf} << 8) | (uint64_t{0xf} << 60);
  REQUIRE((present & requested) == requested);
  probe.access(0, requested);
  REQUIRE(probe.lower.packet_count() == 2);
}

TEST_CASE("UBS frontend forwards the union of instruction bytes to the cache channel")
{
  ubs_probe probe;
  O3_CPU core{champsim::core_builder{}.fetch_queues(&probe.upper.queues).l1i(&probe.cache)};
  for (const auto offset : {0u, 8u, 62u}) {
    auto instruction = champsim::test::instruction_with_ip(0x10000 + offset);
    core.IFETCH_BUFFER.push_back(instruction);
  }
  REQUIRE(core.do_fetch_instruction(core.IFETCH_BUFFER.begin(), core.IFETCH_BUFFER.end()));
  REQUIRE(probe.upper.queues.RQ.size() == 1);
  REQUIRE(probe.upper.queues.RQ.front().requested_bytes == (0xfull | (uint64_t{0xf} << 8) | (uint64_t{3} << 62)));
  REQUIRE(probe.upper.queues.RQ.front().instr_depend_on_me.size() == 3);
}

TEST_CASE("UBS partial misses transfer all resident bytes through merged MSHRs back to the predictor")
{
  ubs_probe probe{true, 4, 8, 100};
  const champsim::address line{0x10000};
  // Seed fragments with known physical coverage, including bytes absent from
  // both new requests. The miss must preserve the whole resident-byte mask.
  for (const auto way : {0u, 2u}) {
    auto& block = probe.cache.block.at(way);
    block.valid = true;
    block.address = line;
    block.v_address = line;
    block.size = static_cast<uint8_t>(sweep_layout.at(way));
    block.offset = static_cast<uint8_t>(way == 0 ? 0 : 16);
  }
  const auto resident = 0xfull | (uint64_t{0xff} << 16);
  const auto first_request = uint64_t{0xf} << 32;
  const auto second_request = uint64_t{0xf} << 48;

  probe.issue(0, first_request);
  run_cycles(probe.elements, 10);
  REQUIRE(probe.cache.MSHR.size() == 1);
  REQUIRE(probe.cache.MSHR.front().ubs_preserved_bytes == resident);
  REQUIRE(probe.cache.MSHR.front().requested_bytes == first_request);
  REQUIRE(std::none_of(probe.cache.block.begin(), probe.cache.block.end(), [](const auto& b) { return b.valid; }));

  // The original fragments are now gone. A second miss must retain the first
  // miss's snapshot even though its own tag lookup finds no resident bytes.
  probe.issue(0, second_request);
  run_cycles(probe.elements, 10);
  REQUIRE(probe.lower.packet_count() == 1);
  REQUIRE(probe.cache.MSHR.size() == 1);
  REQUIRE(probe.cache.MSHR.front().ubs_preserved_bytes == resident);
  REQUIRE(probe.cache.MSHR.front().requested_bytes == (first_request | second_request));
  run_cycles(probe.elements, 140);
  REQUIRE(probe.cache.MSHR.empty());

  const auto all_bytes = resident | first_request | second_request;
  REQUIRE(probe.lower.packet_count() == 1);
  // Do not re-access the old bytes before eviction: that would teach the mask
  // again and conceal a failure to restore it from the MSHR.
  probe.issue(1, 0xf);
  run_cycles(probe.elements, 140); // drain predictor to sub-block storage
  uint64_t stored = 0;
  for (const auto& block : probe.cache.block)
    if (block.valid && block.address == line)
      stored |= low_bytes(block.size) << block.offset;
  REQUIRE((stored & all_bytes) == all_bytes);
  probe.access(0, all_bytes);
  REQUIRE(probe.lower.packet_count() == 2);
}

