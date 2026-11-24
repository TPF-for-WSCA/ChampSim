#include <catch.hpp>

#include "dram_controller.h"

TEST_CASE("DRAM controller counts forwarding without a device read and records the later write burst")
{
  using event = champsim::dram_event;
  using champsim::chrono::picoseconds;

  champsim::channel input{4, 4, 4, champsim::data::bits{6}, false};
  MEMORY_CONTROLLER controller{picoseconds{416}, picoseconds{833}, 16, 16, 16, 39, champsim::chrono::microseconds{32000},
                               {&input}, 4, 4, 1, champsim::data::bytes{8}, 32768, 1024, 1, 4, 4, 8192};
  controller.warmup = false;
  controller.begin_phase();
  auto& activity = controller.channels.at(0).energy_activity.emplace();

  champsim::channel::request_type read;
  read.address = champsim::address{0x8000};
  read.type = access_type::LOAD;
  auto write = read;
  write.type = access_type::WRITE;
  write.data = champsim::address{1234};
  REQUIRE(input.add_rq(read));
  REQUIRE(input.add_wq(write));
  controller._operate();

  REQUIRE(input.returned.size() == 1);
  CHECK(input.returned.front().data == write.data);
  CHECK(activity[event::read_enqueue] == 1);
  CHECK(activity[event::write_enqueue] == 1);
  CHECK(activity[event::forward] == 1);
  CHECK(activity[event::response] == 1);
  CHECK(activity[event::read_release] == 1);
  CHECK(activity[event::read_burst] == 0);

  for (int cycle = 0; cycle < 200; ++cycle)
    controller._operate();
  CHECK(activity[event::write_burst] == 1);
  CHECK(activity[event::write_release] == 1);
  CHECK(activity[event::reserve_bank] == 1);
  CHECK(activity[event::turn] == 2);
}
