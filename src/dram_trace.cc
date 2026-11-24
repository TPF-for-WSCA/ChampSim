#include "dram_trace.h"

#include <nlohmann/json.hpp>

#include "dram_controller.h"

namespace champsim
{
dram_trace::dram_trace(const std::string& path, const MEMORY_CONTROLLER& controller) : output(path)
{
  output.exceptions(std::ios::badbit | std::ios::failbit);
  const auto& channel = controller.channels.at(0);
  const auto& map = channel.address_mapping;
  const nlohmann::json geometry{
      {"format", "champsim-dram-events-v1"}, {"channels", controller.channels.size()},
      {"ranks", map.ranks()}, {"bankgroups", map.bankgroups()}, {"banks_per_group", map.banks()},
      {"rows", map.rows()}, {"columns", map.columns()}, {"channel_width_bits", 8 * channel.channel_width.count()},
      {"burst_length", map.prefetch_size}, {"clock_ps", channel.clock_period.count()},
      {"beat_ps", channel.data_bus_period.count()}, {"refresh_interval_ps", channel.tREF.count()}};
  output << geometry.dump() << '\n';
  output << "event,time_ps,channel,rank,bankgroup,bank,row,column\n";
}

void dram_trace::begin(chrono::clock::time_point time)
{
  enabled = true;
  event("begin", time, 0);
}

void dram_trace::end(chrono::clock::time_point time)
{
  event("end", time, 0);
  enabled = false;
  output.flush();
}

void dram_trace::event(std::string_view kind, chrono::clock::time_point time, std::size_t channel, std::size_t rank,
                       std::size_t bankgroup, std::size_t bank, std::size_t row, std::size_t column)
{
  if (enabled)
    output << kind << ',' << time.time_since_epoch().count() << ',' << channel << ',' << rank << ',' << bankgroup << ',' << bank
           << ',' << row << ',' << column << '\n';
}
} // namespace champsim
