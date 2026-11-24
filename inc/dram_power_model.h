#ifndef CHAMPSIM_DRAM_POWER_MODEL_H
#define CHAMPSIM_DRAM_POWER_MODEL_H

#include "energy_stats.h"

#include <cstddef>
#include <memory>

namespace champsim
{
struct dram_power_geometry {
  std::size_t channels, ranks, groups, banks_per_group, rows, columns, width_bits, burst_length;
  chrono::picoseconds clock, beat, refresh_interval;
};

struct dram_power_burst {
  chrono::clock::time_point reserved, data_start;
  std::size_t channel, rank, group, bank, row, column;
  bool write;
};

class dram_power_model
{
public:
  virtual ~dram_power_model() = default;
  virtual void begin(chrono::clock::time_point time) = 0;
  virtual void observe(dram_power_burst burst) = 0;
  [[nodiscard]] virtual dram_replay_energy_stats finish(chrono::clock::time_point time) = 0;
};

using dram_power_factory = std::unique_ptr<dram_power_model> (*)(dram_power_geometry, const std::string&, double);
std::unique_ptr<dram_power_model> make_linked_drampower(dram_power_geometry geometry, const std::string& memspec, double toggle);
} // namespace champsim

#endif
