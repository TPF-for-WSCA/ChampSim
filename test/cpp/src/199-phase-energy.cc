#include <sstream>
#include <catch.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <nlohmann/json.hpp>

#include "stats_printer.h"

TEST_CASE("Phase energy uses the common simulated interval and sums the modeled components")
{
  using champsim::chrono::picoseconds;
  using Catch::Matchers::WithinRel;
  using time_point = champsim::chrono::clock::time_point;

  champsim::phase_stats phase;
  phase.energy = champsim::phase_energy_stats{
      time_point{picoseconds{1000}},
      time_point{picoseconds{3000}},
      4,
      {{"CPU 0 branch predictor 0", "model", "revision", "assumptions", picoseconds{250}, 1e-12},
       {"CPU 1 branch predictor 0", "model", "revision", "assumptions", picoseconds{345}, 3e-12}}};

  std::ostringstream stream;
  std::vector<champsim::phase_stats> phases{phase};
  champsim::json_printer{stream}.print(phases);
  const auto result = nlohmann::json::parse(stream.str());
  const auto& energy = result.at(0).at("energy");

  REQUIRE(energy.at("coverage") == "partial");
  REQUIRE(energy.at("begin_ps") == 1000);
  REQUIRE(energy.at("end_ps") == 3000);
  REQUIRE_THAT(energy.at("modeled_dynamic_energy_joules").get<double>(), WithinRel(4e-12));
  REQUIRE_THAT(energy.at("modeled_average_dynamic_power_watts").get<double>(), WithinRel(2e-3));
  REQUIRE_THAT(energy.at("modeled_dynamic_energy_per_instruction_joules").get<double>(), WithinRel(1e-12));
  REQUIRE(energy.at("components").at(1).at("clock_period_ps") == 345);
  REQUIRE_THAT(energy.at("components").at(1).at("average_dynamic_power_watts").get<double>(), WithinRel(1.5e-3));
}

TEST_CASE("Average dynamic power is unavailable for an empty interval")
{
  const champsim::phase_energy_stats energy{};
  REQUIRE_FALSE(energy.average_dynamic_power_watts(0).has_value());
}
