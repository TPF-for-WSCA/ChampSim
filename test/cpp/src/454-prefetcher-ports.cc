#include <catch.hpp>
#include "cache.h"
#include "defaults.hpp"
#include "../../../prefetcher/berti/berti.h"
#include "../../../prefetcher/pythia/pythia.h"
#include "../../../prefetcher/pythia/upstream/scooby.hpp"

TEST_CASE("Berti requires its virtual address and page geometry contract", "[prefetcher-ports]")
{
  CACHE physical{champsim::cache_builder{champsim::defaults::default_l1d}.sets(64)};
  berti predictor{&physical};
  REQUIRE_THROWS_AS(predictor.prefetcher_initialize(), std::invalid_argument);
}

TEST_CASE("Berti learns repeated streams and remains bound after a module move", "[prefetcher-ports]")
{
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1d}.sets(64).set_virtual_prefetch().pq_size(65536)};
  berti original{&cache};
  original.prefetcher_initialize();
  berti predictor{std::move(original)};
  for (uint64_t i = 0; i < 4096; ++i) {
    const champsim::address addr{0x100000 + (i % 1024) * 64};
    cache.current_time += 40 * cache.clock_period;
    REQUIRE(predictor.prefetcher_cache_operate(addr, champsim::address{0x1000}, false, false, access_type::LOAD, 123) == 123);
    cache.current_time += 20 * cache.clock_period;
    REQUIRE(predictor.prefetcher_cache_fill(addr, 0, 0, false, {}, 123) == 123);
  }
  REQUIRE(cache.sim_stats.pf_requested > 0);
  REQUIRE(cache.sim_stats.pf_issued > 0);
}

TEST_CASE("Pythia generates requests and handles useful hits and fills", "[prefetcher-ports]")
{
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l2c}.sets(512).pq_size(65536)};
  pythia predictor{&cache};
  predictor.prefetcher_initialize();
  for (uint64_t i = 0; i < 4096; ++i) {
    const champsim::address addr{0x100000 + (i % 1024) * 64};
    cache.current_time += 10 * cache.clock_period;
    REQUIRE(predictor.prefetcher_cache_operate(addr, champsim::address{0x1000}, i % 2, i % 2, access_type::LOAD, 456) == 456);
    REQUIRE(predictor.prefetcher_cache_fill(addr, 0, 0, true, {}, 456) == 456);
    predictor.prefetcher_cycle_operate();
  }
  REQUIRE(cache.sim_stats.pf_requested > 0);
  REQUIRE(cache.sim_stats.pf_issued > 0);
}

TEST_CASE("Pythia learning state is independent between caches", "[prefetcher-ports]")
{
  pythia_impl::Scooby first{"first"}, second{"second"}, noise{"noise"};
  for (uint64_t i = 0; i < 2048; ++i) {
    const uint64_t addr = 0x100000 + (i % 1024) * 64;
    std::vector<uint64_t> a, b, other;
    noise.invoke_prefetcher(0x2000 + i % 16, 0x800000 + (i * 17 % 4096) * 64, false, 0, other);
    first.invoke_prefetcher(0x1000, addr, false, 0, a);
    second.invoke_prefetcher(0x1000, addr, false, 0, b);
    REQUIRE(a == b);
    first.register_fill(addr);
    second.register_fill(addr);
  }
}

TEST_CASE("Prefetch issuing PCs reach the lower cache and survive queue rejection", "[prefetcher-ports]")
{
  champsim::channel lower;
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1d}.sets(64).lower_level(&lower).pq_size(1)};
  cache.initialize();
  cache.warmup = false;
  const champsim::address pc{0x400100}, addr{0x100000};
  REQUIRE(cache.prefetch_line(addr, true, 789, pc));
  REQUIRE_FALSE(cache.prefetch_line(champsim::address{0x200000}, true, 0, champsim::address{0xdead}));
  for (int cycle = 0; cycle < 20; ++cycle) cache._operate();
  REQUIRE(lower.PQ.size() == 1);
  REQUIRE(lower.PQ.front().ip == pc);
  REQUIRE(lower.PQ.front().address == addr);
  REQUIRE(lower.PQ.front().pf_metadata == 789);
}
