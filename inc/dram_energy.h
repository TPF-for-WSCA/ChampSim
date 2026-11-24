#ifndef CHAMPSIM_DRAM_ENERGY_H
#define CHAMPSIM_DRAM_ENERGY_H

#include <array>
#include <cstddef>
#include <cstdint>

#include "energy_stats.h"

namespace champsim
{
enum class dram_event {
  ingress, read_enqueue, write_enqueue, read_release, write_release, collision_compare, checked, merge, forward,
  arbitrate, candidate, reserve_bank, release_bank, cancel_bank, row_compare, turn, refresh_due, refresh_start, refresh_end,
  bus_arbitrate, bus_candidate, read_burst, write_burst, response, count
};

struct dram_activity {
  std::array<uint64_t, static_cast<std::size_t>(dram_event::count)> counts{};
  void record(dram_event event, std::size_t count) { counts[static_cast<std::size_t>(event)] += count; }
  [[nodiscard]] uint64_t operator[](dram_event event) const { return counts[static_cast<std::size_t>(event)]; }
};

class dram_energy_model
{
public:
  virtual ~dram_energy_model() = default;
  [[nodiscard]] virtual std::vector<component_energy_stats> energy_snapshots() const = 0;
};

struct environment;
void configure_harcom_dram(environment& env);
} // namespace champsim

#endif
