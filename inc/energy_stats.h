#ifndef CHAMPSIM_ENERGY_STATS_H
#define CHAMPSIM_ENERGY_STATS_H

#include <chrono>
#include <cstdint>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "chrono.h"

namespace champsim
{
struct energy_model_snapshot {
  std::string_view model;
  std::string_view revision;
  std::string_view assumptions;
  double dynamic_energy_fJ;
  std::optional<double> leakage_power_watts{};
};

struct component_energy_stats {
  std::string instance;
  std::string_view model;
  std::string_view revision;
  std::string assumptions;
  chrono::picoseconds clock_period;
  double dynamic_energy_joules;
  std::optional<double> leakage_power_watts{};
};

struct dram_replay_burst_delay {
  std::uint64_t bursts;
  double mean_ps;
  double max_ps;
};

struct dram_replay_energy_stats {
  double native_duration_ps;
  double replay_duration_ps;
  double energy_joules;
  std::vector<double> channel_energy_joules;
  std::string memspec_sha256;
  std::string cli_sha256;
  double toggle_rate;
  std::string method = "DRAMPower 6.0 serialized command replay";
  std::string library_revision;
  std::string memspec_file;
  std::optional<dram_replay_burst_delay> burst_delay{};

  [[nodiscard]] double elapsed_delta_ps() const { return replay_duration_ps - native_duration_ps; }
  [[nodiscard]] double average_power_watts() const { return energy_joules / (replay_duration_ps * 1e-12); }
};

inline constexpr std::string_view leakage_assumptions =
    "HARCOM nominal 0.75 V fin model with 25 C reference off-currents, modeled structures powered throughout the simulated interval";

struct phase_energy_stats {
  chrono::clock::time_point begin;
  chrono::clock::time_point end;
  long long instructions{};
  std::vector<component_energy_stats> components;
  std::vector<std::string> coverage_gaps{};
  std::optional<dram_replay_energy_stats> dram_replay{};

  [[nodiscard]] double modeled_dynamic_energy_joules() const
  {
    return std::accumulate(components.begin(), components.end(), 0.0,
                           [](double sum, const auto& component) { return sum + component.dynamic_energy_joules; });
  }

  [[nodiscard]] double elapsed_seconds() const { return std::chrono::duration<double>(end - begin).count(); }

  [[nodiscard]] std::optional<double> leakage_energy_joules(const component_energy_stats& component) const
  {
    if (!component.leakage_power_watts)
      return std::nullopt;
    return *component.leakage_power_watts * elapsed_seconds();
  }

  [[nodiscard]] double modeled_leakage_energy_joules() const
  {
    return std::accumulate(components.begin(), components.end(), 0.0,
                           [this](double sum, const auto& component) { return sum + leakage_energy_joules(component).value_or(0); });
  }

  [[nodiscard]] double modeled_on_chip_energy_joules() const
  {
    return modeled_dynamic_energy_joules() + modeled_leakage_energy_joules();
  }

  [[nodiscard]] double combined_modeled_energy_joules() const
  {
    return modeled_on_chip_energy_joules() + (dram_replay ? dram_replay->energy_joules : 0);
  }

  [[nodiscard]] std::optional<double> average_power_watts(double energy_joules) const
  {
    const auto elapsed = elapsed_seconds();
    if (elapsed <= 0)
      return std::nullopt;
    return energy_joules / elapsed;
  }

  [[nodiscard]] std::optional<double> average_dynamic_power_watts(double energy_joules) const { return average_power_watts(energy_joules); }
};
} // namespace champsim

#endif
