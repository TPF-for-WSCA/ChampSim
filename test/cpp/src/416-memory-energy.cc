#include <catch.hpp>

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
}
