#undef CHAMPSIM_MODULE
#include "../sram_cost.h"
#include "environment.h"
#include "storage.h"

namespace champsim
{
namespace
{
constexpr std::string_view revision = "5ceea8358ddbab94e5621e2536530b860cc1e5b7";
constexpr auto flop = hcm::flipflops<1>::write_energy_fJ;
constexpr auto gate = hcm::AND<2>.e;
using event = dram_event;
using harcom::sram_row;
using harcom::match_energy;

struct meter {
  const DRAM_CHANNEL& channel;
  std::string name;
  std::string_view assumptions;
  std::vector<std::pair<event, double>> terms;

  component_energy_stats sample() const
  {
    double femtojoules = 0;
    for (const auto& [op, cost] : terms)
      femtojoules += static_cast<double>((*channel.energy_activity)[op]) * cost;
    return {name, "HARCOM DRAM controller", revision, std::string{assumptions}, channel.clock_period, femtojoules * 1e-15};
  }
};

class controller_model final : public dram_energy_model
{
  std::vector<meter> meters;

public:
  explicit controller_model(MEMORY_CONTROLLER& controller)
  {
    for (std::size_t i = 0; i < controller.channels.size(); ++i) {
      auto& channel = controller.channels[i];
      channel.energy_activity.emplace();
      const auto prefix = fmt::format("DRAM channel {} ", i);
      const auto& geometry = channel.address_mapping;
      const auto read_depth = channel.RQ.size();
      const auto write_depth = channel.WQ.size();
      const auto bank_count = channel.bank_request.size();
      const auto queue_bits = std::max(storage_bits(read_depth), storage_bits(write_depth));
      const auto row_bits = storage_bits(geometry.rows());
      const auto block_bits = 64 - LOG2_BLOCK_SIZE;
      const auto payload = 8 * BLOCK_SIZE;
      const auto read = sram_row(read_depth, block_bits + 32 + 3);
      const auto write = sram_row(write_depth, block_bits + 32 + 3);
      const auto data = sram_row(write_depth, payload);
      const auto select_slot = queue_bits == 0 ? 0 : hcm::anding(queue_bits, hcm::OUTCAP).e;
      // The queue uses compact slot indices and local valid/checked state.
      // Simulator timestamps and dependency vectors are excluded.
      meters.push_back({channel, prefix + "request queues",
                        "Address and metadata SRAM, write data SRAM, valid and checked bits, forwarding and merge",
                        {{event::ingress, select_slot + gate},
                         {event::read_enqueue, read.write + flop}, {event::write_enqueue, write.write + data.write + flop},
                         {event::read_release, select_slot + flop}, {event::write_release, select_slot + flop},
                         {event::collision_compare, match_energy(block_bits) + gate}, {event::checked, flop},
                         {event::merge, read.read + read.write}, {event::forward, read.read + data.read},
                         {event::read_burst, read.read}, {event::write_burst, write.read + data.read}}});

      const auto bank_bits = storage_bits(bank_count);
      const auto bank_state = sram_row(bank_count, row_bits + queue_bits + 5);
      const auto bank_select = bank_bits == 0 ? 0 : hcm::anding(bank_bits, hcm::OUTCAP).e;
      meters.push_back({channel, prefix + "scheduling and refresh",
                        "Queue candidate arbitration, open-row and bank state, refresh start/end and read/write drain policy",
                        {{event::arbitrate, select_slot + gate}, {event::candidate, 2 * gate + match_energy(queue_bits)},
                         {event::reserve_bank, bank_state.write}, {event::row_compare, bank_state.read + match_energy(row_bits)},
                         {event::release_bank, bank_select + flop}, {event::cancel_bank, bank_select + 2 * flop},
                         {event::turn, gate + flop}, {event::refresh_due, static_cast<double>(bank_count + row_bits) * flop},
                         {event::refresh_start, bank_select + 3 * flop}, {event::refresh_end, bank_select + 2 * flop}}});

      const auto response_bits = static_cast<double>(payload + 2 * 64 + 32);
      const auto group = sram_row(channel.bankgroup_readytime.size(), bank_bits + 1);
      meters.push_back({channel, prefix + "interface control",
                        "Digital bus arbiter and turnaround state, burst command staging and response registers at the controller endpoint",
                        {{event::bus_arbitrate, bank_select + gate}, {event::bus_candidate, 2 * gate + match_energy(bank_bits)},
                         {event::turn, bank_select + flop}, {event::read_burst, group.read + group.write + (block_bits + 1) * flop},
                         {event::write_burst, group.read + group.write + (block_bits + 1) * flop},
                         {event::response, response_bits * flop}}});
    }
  }

  std::vector<component_energy_stats> energy_snapshots() const override
  {
    std::vector<component_energy_stats> result;
    result.reserve(meters.size());
    for (const auto& meter : meters)
      result.push_back(meter.sample());
    return result;
  }
};
} // namespace

void configure_harcom_dram(environment& env) { env.dram_energy = std::make_unique<controller_model>(env.dram_view()); }
} // namespace champsim
