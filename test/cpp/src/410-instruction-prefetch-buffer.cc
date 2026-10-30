#include <catch.hpp>

#include <array>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"

namespace
{
auto buffer_request(uint64_t address, access_type type, uint64_t bytes = 0xf)
{
  champsim::channel::request_type packet;
  packet.address = champsim::address{address};
  packet.v_address = packet.address;
  packet.ip = packet.address;
  packet.cpu = 0;
  packet.type = type;
  packet.is_translated = true;
  packet.requested_bytes = bytes;
  return packet;
}

template <typename Range>
void buffer_cycles(Range& elements, int cycles = 30)
{
  for (int cycle = 0; cycle < cycles; ++cycle)
    for (auto* element : elements)
      element->_operate();
}
}

TEST_CASE("Instruction prefetches promote on demand without polluting either cache organization")
{
  const bool use_ubs = GENERATE(false, true);
  do_nothing_MRC lower;
  to_rq_MRP upper;
  auto builder = champsim::cache_builder{champsim::defaults::default_l1i}
                     .sets(1).ways(1).prefetch_buffer_size(2)
                     .upper_levels({&upper.queues}).lower_level(&lower.queues)
                     .hit_latency(1).fill_latency(1).ubs_way_sizes({4, 8, 64});
  CACHE cache{use_ubs ? builder.set_ubs() : builder.reset_ubs()};
  std::array<champsim::operable*, 3> elements{&lower, &cache, &upper};
  for (auto* element : elements) {
    element->initialize();
    element->warmup = false;
    element->begin_phase();
  }
  auto access = [&](uint64_t address, access_type type, uint64_t bytes = 0xf) {
    REQUIRE(upper.issue(buffer_request(address, type, bytes)));
    buffer_cycles(elements);
    REQUIRE(upper.packets.back().return_time > upper.packets.back().issue_time);
  };
  access(0x1000, access_type::LOAD);
  CHECK(cache.is_instruction_present(champsim::address{0x1020})); // Full predictor/cache line.
  access(0x1040, access_type::PREFETCH);
  const auto useful_before_probe = cache.sim_stats.pf_useful;
  CHECK(cache.is_instruction_present(champsim::address{0x1060})); // Full prefetch-buffer line.
  CHECK(cache.is_instruction_present(champsim::address{0x1060}));
  CHECK(cache.sim_stats.pf_useful == useful_before_probe);
  const auto after_prefetch = lower.packet_count();
  // A's full predictor/cache line survives a conflicting prefetch fill.
  access(0x1020, access_type::LOAD, uint64_t{0xf} << 32);
  CHECK(lower.packet_count() == after_prefetch);
  const auto demand_fills = cache.sim_stats.fill.value_or(std::pair{access_type::LOAD, 0u}, 0);
  const auto demand_misses = cache.sim_stats.misses.value_or(std::pair{access_type::LOAD, 0u}, 0);
  access(0x1040, access_type::LOAD);
  CHECK(lower.packet_count() == after_prefetch);
  CHECK(cache.sim_stats.pf_useful == 1);
  CHECK(cache.sim_stats.misses.value_or(std::pair{access_type::LOAD, 0u}, 0) == demand_misses);
  CHECK(cache.sim_stats.partial_misses.value_or(std::pair{access_type::LOAD, 0u}, 0) == 0);
  CHECK(cache.sim_stats.fill.value_or(std::pair{access_type::LOAD, 0u}, 0) == demand_fills);
  // More than a buffer's worth of new prefetches cannot evict promoted B.
  access(0x1080, access_type::PREFETCH);
  access(0x10c0, access_type::PREFETCH);
  access(0x1100, access_type::PREFETCH);
  CHECK(cache.sim_stats.pf_useless == 1);
  const auto before_reuse = lower.packet_count();
  access(0x1060, access_type::LOAD, uint64_t{0xf} << 32);
  CHECK(lower.packet_count() == before_reuse);
  CHECK(cache.sim_stats.pf_useful == 1);
}

TEST_CASE("Prefetch buffer probes do not consume entries or refresh FIFO order")
{
  const bool use_ubs = GENERATE(false, true);
  do_nothing_MRC lower;
  to_rq_MRP upper;
  auto builder = champsim::cache_builder{champsim::defaults::default_l1i}
                     .sets(1).ways(1).prefetch_buffer_size(2)
                     .upper_levels({&upper.queues}).lower_level(&lower.queues)
                     .hit_latency(1).fill_latency(1);
  CACHE cache{use_ubs ? builder.set_ubs() : builder.reset_ubs()};
  std::array<champsim::operable*, 3> elements{&lower, &cache, &upper};
  for (auto* element : elements) {
    element->initialize();
    element->warmup = false;
    element->begin_phase();
  }
  auto access = [&](uint64_t address, access_type type) {
    REQUIRE(upper.issue(buffer_request(address, type)));
    buffer_cycles(elements);
  };
  access(0x1000, access_type::PREFETCH);
  access(0x1040, access_type::PREFETCH);
  access(0x1000, access_type::PREFETCH);
  CHECK(lower.packet_count() == 2);
  CHECK(cache.sim_stats.pf_useful == 0);
  access(0x1080, access_type::PREFETCH);
  CHECK(cache.sim_stats.pf_useless == 1);
  access(0x1000, access_type::LOAD);
  CHECK(lower.packet_count() == 4); // FIFO evicted A despite the prefetch probe.
  access(0x1040, access_type::LOAD);
  CHECK(lower.packet_count() == 4);
  CHECK(cache.sim_stats.pf_useful == 1);
  cache.invalidate_entry(champsim::address{0x1080});
  access(0x1080, access_type::LOAD);
  CHECK(lower.packet_count() == 5);
}

TEST_CASE("Demand merging into an inflight prefetch inserts directly into either cache")
{
  const bool use_ubs = GENERATE(false, true);
  do_nothing_MRC lower{20};
  to_rq_MRP upper;
  auto builder = champsim::cache_builder{champsim::defaults::default_l1i}
                     .sets(1).ways(1).upper_levels({&upper.queues}).lower_level(&lower.queues)
                     .hit_latency(1).fill_latency(1).reset_virtual_prefetch();
  CACHE cache{use_ubs ? builder.set_ubs() : builder.reset_ubs()};
  std::array<champsim::operable*, 3> elements{&lower, &cache, &upper};
  for (auto* element : elements) {
    element->initialize();
    element->warmup = false;
    element->begin_phase();
  }
  REQUIRE(cache.prefetch_line(champsim::address{0x1000}, true, 0));
  buffer_cycles(elements, 5);
  REQUIRE(upper.issue(buffer_request(0x1020, access_type::LOAD, uint64_t{0xf} << 32)));
  buffer_cycles(elements, 60);
  CHECK(lower.packet_count() == 1);
  CHECK(cache.sim_stats.pf_useful == 1);
  CHECK(cache.sim_stats.pf_fill == 0);
  REQUIRE(upper.issue(buffer_request(0x1030, access_type::LOAD, uint64_t{0xf} << 48)));
  buffer_cycles(elements);
  CHECK(lower.packet_count() == 1);
  CHECK(cache.sim_stats.pf_useful == 1);
}

TEST_CASE("Instruction prefetch buffers are enabled by default only for instruction caches")
{
  CACHE instruction{champsim::cache_builder{champsim::defaults::default_l1i}.sets(1)};
  CACHE data{champsim::cache_builder{champsim::defaults::default_l1d}.sets(1)};
  CACHE disabled{champsim::cache_builder{champsim::defaults::default_l1i}.sets(1).prefetch_buffer_size(0)};
  CHECK(instruction.PREFETCH_BUFFER_SIZE == 32);
  CHECK(data.PREFETCH_BUFFER_SIZE == 0);
  CHECK(disabled.PREFETCH_BUFFER_SIZE == 0);
}

TEST_CASE("Buffered prefetch partial misses preserve UBS fragments until demand promotion")
{
  do_nothing_MRC lower;
  to_rq_MRP upper;
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1i}
                  .sets(1).set_ubs().ubs_way_sizes({4, 8, 64})
                  .upper_levels({&upper.queues}).lower_level(&lower.queues)
                  .hit_latency(1).fill_latency(1)};
  std::array<champsim::operable*, 3> elements{&lower, &cache, &upper};
  for (auto* element : elements) {
    element->initialize();
    element->warmup = false;
    element->begin_phase();
  }
  auto access = [&](uint64_t address, access_type type, uint64_t mask = 0xf) {
    REQUIRE(upper.issue(buffer_request(address, type, mask)));
    buffer_cycles(elements);
  };
  access(0x1000, access_type::LOAD);
  access(0x1040, access_type::LOAD); // A's first four bytes enter subblock storage.
  access(0x1008, access_type::PREFETCH, uint64_t{0xf} << 8);
  CHECK(cache.sim_stats.partial_misses.value_or(std::pair{access_type::PREFETCH, 0u}, 0) == 1);
  CHECK(std::count_if(cache.block.begin(), cache.block.end(), [](const auto& block) {
    return block.valid && champsim::block_number{block.address} == champsim::block_number{champsim::address{0x1000}};
  }) == 1);
  const auto requests = lower.packet_count();
  access(0x1000, access_type::LOAD); // Resident subblock hit does not consume A's buffer entry.
  CHECK(cache.sim_stats.pf_useful == 0);
  access(0x1020, access_type::LOAD, uint64_t{0xf} << 32); // Promote buffered A.
  CHECK(cache.sim_stats.pf_useful == 1);
  access(0x1080, access_type::LOAD); // Evict predictor A, retaining old and new useful bytes.
  access(0x1000, access_type::LOAD, uint64_t{0xf} | (uint64_t{0xf} << 32));
  CHECK(lower.packet_count() == requests + 1);
}

TEST_CASE("Zero-sized prefetch buffer retains direct cache insertion")
{
  const bool use_ubs = GENERATE(false, true);
  do_nothing_MRC lower;
  to_rq_MRP upper;
  auto builder = champsim::cache_builder{champsim::defaults::default_l1i}
                     .sets(1).ways(1).prefetch_buffer_size(0).ubs_way_sizes({4, 8, 64})
                     .upper_levels({&upper.queues}).lower_level(&lower.queues)
                     .hit_latency(1).fill_latency(1);
  CACHE cache{use_ubs ? builder.set_ubs() : builder.reset_ubs()};
  std::array<champsim::operable*, 3> elements{&lower, &cache, &upper};
  for (auto* element : elements) {
    element->initialize();
    element->warmup = false;
    element->begin_phase();
  }
  REQUIRE(upper.issue(buffer_request(0x1000, access_type::LOAD)));
  buffer_cycles(elements);
  REQUIRE(upper.issue(buffer_request(0x1040, access_type::PREFETCH)));
  buffer_cycles(elements);
  const auto requests = lower.packet_count();
  REQUIRE(upper.issue(buffer_request(0x1020, access_type::LOAD, uint64_t{0xf} << 32)));
  buffer_cycles(elements);
  CHECK(lower.packet_count() == requests + 1);
}
