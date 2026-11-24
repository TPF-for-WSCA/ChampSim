#ifndef CHAMPSIM_DRAM_POWER_PROCESS_H
#define CHAMPSIM_DRAM_POWER_PROCESS_H

#include <string>
#include <string_view>

#include "energy_stats.h"

namespace champsim
{
class dram_power_process
{
  int input = -1;
  int output = -1;
  int child = -1;
  std::string pending;

  void flush();
  int wait() noexcept;

public:
  dram_power_process(const std::string& adapter, const std::string& cli, const std::string& memspec, double toggle);
  ~dram_power_process();
  dram_power_process(const dram_power_process&) = delete;
  dram_power_process& operator=(const dram_power_process&) = delete;

  void send(std::string_view event);
  [[nodiscard]] dram_replay_energy_stats finish();
};
} // namespace champsim

#endif
