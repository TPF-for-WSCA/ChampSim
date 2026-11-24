#include <catch.hpp>

#include "../../../replacement/drrip/drrip.h"
#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"

TEST_CASE("Memory energy activity distinguishes queue admission, MSHR merges and selected data accesses")
{
  using champsim::memory_event;
  using champsim::request_queue;

  do_nothing_MRC memory{8};
  champsim::channel input{2, 1, 1, champsim::data::bits{6}, false};
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1d}
                  .sets(1)
                  .ways(1)
                  .hit_latency(2)
                  .fill_latency(2)
                  .upper_levels({&input})
                  .lower_level(&memory.queues)};
  cache.warmup = false;
  cache.initialize();
  cache.memory_activity.emplace();
  input.energy_activity.emplace();

  const auto drain = [&] {
    for (int cycle = 0; cycle < 50; ++cycle) {
      cache._operate();
      memory._operate();
    }
  };
  const auto& activity = *cache.memory_activity;
  const auto& read_queue = input.energy_activity->at(static_cast<std::size_t>(request_queue::read));
  const auto& write_queue = input.energy_activity->at(static_cast<std::size_t>(request_queue::write));

  champsim::channel::request_type read{};
  read.address = champsim::address{0x1000};
  read.cpu = 0;
  REQUIRE(input.add_rq(read));
  REQUIRE(input.add_rq(read));
  CHECK_FALSE(input.add_rq(read));
  CHECK(read_queue.writes == 2);
  CHECK(read_queue.reads == 0);
  drain();
  CHECK(read_queue.reads == 2);
  CHECK(activity[memory_event::tag_lookup] == 2);
  CHECK(activity[memory_event::mshr_write] == 2); // Allocation, then merge.
  CHECK(activity[memory_event::mshr_release] == 1);
  CHECK(activity[memory_event::fill_stage_write] == 1);
  CHECK(activity[memory_event::fill_stage_read] == 1);
  CHECK(activity[memory_event::fill_stage_release] == 1);
  CHECK(activity[memory_event::response_delivery] == 1); // Same requester merged.
  CHECK(activity[memory_event::data_write] == 1); // One fill for both requests.
  CHECK(activity[memory_event::data_read] == 0);

  REQUIRE(input.add_rq(read));
  drain();
  CHECK(activity[memory_event::data_read] == 1);

  auto write = read;
  write.response_requested = false;
  write.type = access_type::WRITE;
  REQUIRE(input.add_wq(write));
  drain();
  CHECK(write_queue.writes == 1);
  CHECK(write_queue.reads == 1);
  CHECK(activity[memory_event::data_read] == 1);
  CHECK(activity[memory_event::data_write] == 2);

  read.address = champsim::address{0x2000};
  REQUIRE(input.add_rq(read));
  drain();
  CHECK(activity[memory_event::data_read] == 2); // A dirty victim is read for writeback.
  CHECK(activity[memory_event::data_write] == 3);
  CHECK(activity[memory_event::fill_stage_write] == 2);
  CHECK(activity[memory_event::response_delivery] == 3);
}

TEST_CASE("DRRIP accounting distinguishes sampled fills, followers and writebacks")
{
  using champsim::memory_event;
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1d}.sets(64).ways(2)};
  cache.memory_activity.emplace();
  drrip policy{&cache};
  const auto& activity = *cache.memory_activity;

  for (unsigned index = 0; index < drrip::BRRIP_MAX - 1; ++index) {
    policy.replacement_cache_fill(0, 0, 0, {}, {}, {}, access_type::LOAD);
    CHECK(policy.rrpv[0] == drrip::maxRRPV);
  }
  policy.replacement_cache_fill(0, 0, 0, {}, {}, {}, access_type::LOAD);
  CHECK(policy.rrpv[0] == drrip::maxRRPV - 1);
  CHECK(policy.brrip_counter == 0);
  CHECK(activity[memory_event::drrip_brrip_tick] == drrip::BRRIP_MAX);
  CHECK(activity[memory_event::drrip_selector_update] == drrip::BRRIP_MAX);

  policy.replacement_cache_fill(0, 2, 0, {}, {}, {}, access_type::LOAD);
  policy.replacement_cache_fill(0, 1, 0, {}, {}, {}, access_type::WRITE);
  CHECK(activity[memory_event::drrip_classify] == drrip::BRRIP_MAX + 1);
  CHECK(activity[memory_event::drrip_selector_read] == 1);
  (void)policy.find_victim(0, 0, 3, nullptr, {}, {}, access_type::LOAD);
  (void)policy.find_victim(0, 0, 3, nullptr, {}, {}, access_type::LOAD);
  CHECK(activity[memory_event::drrip_age_row] == 1);
}

TEST_CASE("A full refill queue defers lower-level responses and writebacks")
{
  using champsim::memory_event;
  release_MRC memory;
  champsim::channel input{4, 4, 1, champsim::data::bits{6}, false};
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l2c}
                  .sets(4)
                  .ways(1)
                  .mshr_size(4)
                  .refill_queue_size(1)
                  .hit_latency(1)
                  .fill_latency(4)
                  .tag_bandwidth(champsim::bandwidth::maximum_type{2})
                  .upper_levels({&input})
                  .lower_level(&memory.queues)};
  cache.warmup = false;
  cache.initialize();
  cache.memory_activity.emplace();
  auto& link = memory.queues.link_activity.emplace();

  const auto tick = [&] {
    cache._operate();
    memory._operate();
  };
  const auto& activity = *cache.memory_activity;
  champsim::channel::request_type read{};
  read.address = champsim::address{0x1000};
  read.cpu = 0;
  REQUIRE(input.add_rq(read));
  read.address = champsim::address{0x1040};
  REQUIRE(input.add_rq(read));
  for (auto cycle = 0; cycle < 6; ++cycle)
    tick();
  REQUIRE(cache.get_mshr_occupancy() == 2);
  memory.release_all();
  REQUIRE(memory.queues.returned.size() == 2);

  auto writeback = read;
  writeback.address = champsim::address{0x1080};
  writeback.type = access_type::WRITE;
  writeback.response_requested = false;
  REQUIRE(input.add_wq(writeback));
  tick();
  tick();
  CHECK(memory.queues.returned.size() == 1);
  CHECK(cache.get_mshr_occupancy() == 1);
  CHECK(link.responses == 1);
  CHECK(activity[memory_event::fill_stage_write] == 1);

  for (auto cycle = 0; cycle < 20; ++cycle) {
    tick();
    CHECK(activity[memory_event::fill_stage_write] - activity[memory_event::fill_stage_release] <= 1);
  }
  CHECK(memory.queues.returned.empty());
  CHECK(cache.get_mshr_occupancy() == 0);
  CHECK(link.responses == 2);
  CHECK(input.returned.size() == 2);
  CHECK(activity[memory_event::fill_stage_write] == 3);
  CHECK(activity[memory_event::fill_stage_release] == 3);
}
