#ifndef CHAMPSIM_ENERGY_STATS_H
#define CHAMPSIM_ENERGY_STATS_H

#include <chrono>
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
};

struct component_energy_stats {
  std::string instance;
  std::string_view model;
  std::string_view revision;
  std::string assumptions;
  chrono::picoseconds clock_period;
  double dynamic_energy_joules;
};

struct phase_energy_stats {
  chrono::clock::time_point begin;
  chrono::clock::time_point end;
  long long instructions{};
  std::vector<component_energy_stats> components;
  std::vector<std::string> coverage_gaps{};

  [[nodiscard]] double modeled_dynamic_energy_joules() const
  {
    return std::accumulate(components.begin(), components.end(), 0.0,
                           [](double sum, const auto& component) { return sum + component.dynamic_energy_joules; });
  }

  [[nodiscard]] std::optional<double> average_dynamic_power_watts(double energy_joules) const
  {
    const auto elapsed = std::chrono::duration<double>(end - begin).count();
    if (elapsed <= 0)
      return std::nullopt;
    return energy_joules / elapsed;
  }
};
} // namespace champsim

#endif
