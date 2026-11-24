#include <catch.hpp>

#include "../../../harcom/drampower/scheduler.h"

namespace
{
using namespace champsim::drampower;

timing ddr4_timing()
{
  return {1, 16, 39, 55, 16, 1, 4, 6, 0, 26, 4, 6, 0, 12, 18, 3, 9, 2, 16, 16, 4, 1, 0, 48, 320};
}

std::vector<command> only(const std::vector<command>& commands, operation op)
{
  std::vector<command> result;
  std::copy_if(commands.begin(), commands.end(), std::back_inserter(result), [op](const auto& cmd) { return cmd.op == op; });
  return result;
}
} // namespace

TEST_CASE("Linked DRAM replay overlaps activations and honors the rolling four-ACT limit")
{
  const auto device = ddr4_timing();
  scheduler uut{device, 1, 2, 4};
  for (std::size_t i = 0; i < 5; ++i)
    uut.add({0, 32 + 4 * i, {0, i % 2, i / 2, 10, 0}, false});

  const auto end = uut.schedule(0);
  const auto& commands = uut.finish(end);
  const auto acts = only(commands, operation::activate);
  const auto reads = only(commands, operation::read);
  REQUIRE(acts.size() == 5);
  REQUIRE(reads.size() == 5);
  CHECK(acts[1].when < reads.front().when);
  CHECK(acts[1].when - acts[0].when >= device.rrd_short);
  CHECK(acts[4].when - acts[0].when >= device.faw);
  for (const auto& read : reads) {
    auto activate = std::find_if(acts.begin(), acts.end(), [&](const auto& act) {
      return act.address.group == read.address.group && act.address.bank == read.address.bank;
    });
    REQUIRE(activate != acts.end());
    CHECK(read.when - activate->when >= device.rcd);
  }
  CHECK(end < 100);
  CHECK(uut.burst_start_delays().count == reads.size());
}

TEST_CASE("Linked DRAM replay keeps bank request order across turnaround, row conflicts and refresh")
{
  const auto device = ddr4_timing();
  scheduler uut{device, 1, 2, 4};
  uut.add({0, 32, {0, 0, 0, 1, 0}, true});
  uut.add({0, 32, {0, 0, 0, 1, 8}, false});
  uut.add({0, 32, {0, 0, 0, 2, 0}, false});
  uut.add({320, 352, {0, 0, 0, 2, 8}, false});

  const auto end = uut.schedule(640);
  const auto& commands = uut.finish(end);
  const auto acts = only(commands, operation::activate);
  const auto reads = only(commands, operation::read);
  const auto writes = only(commands, operation::write);
  const auto precharges = only(commands, operation::precharge);
  const auto refreshes = only(commands, operation::refresh);
  REQUIRE(acts.size() == 3);
  REQUIRE(reads.size() == 3);
  REQUIRE(writes.size() == 1);
  REQUIRE(precharges.size() == 1);
  REQUIRE(refreshes.size() == 2);
  CHECK(reads[0].when - writes[0].when >= device.write_latency + device.beats + device.wtr_long);
  CHECK(precharges[0].when - writes[0].when >= device.write_latency + device.beats + device.wr);
  CHECK(acts[1].when - precharges[0].when >= device.rp);
  CHECK(refreshes[1].when - refreshes[0].when == device.refi);
  CHECK(acts[2].when - refreshes[0].when >= device.rfc);
  CHECK(reads[2].address.row == 2);
}
