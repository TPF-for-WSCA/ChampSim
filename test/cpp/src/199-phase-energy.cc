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
      {{"CPU 0 branch predictor 0", "model", "revision", "assumptions", picoseconds{250}, 1e-12, 5e-4},
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
  REQUIRE_THAT(energy.at("modeled_leakage_energy_joules").get<double>(), WithinRel(1e-12));
  REQUIRE_THAT(energy.at("modeled_on_chip_energy_joules").get<double>(), WithinRel(5e-12));
  REQUIRE_THAT(energy.at("modeled_average_on_chip_power_watts").get<double>(), WithinRel(2.5e-3));
  const auto& with_leakage = energy.at("components").at(0);
  REQUIRE_THAT(with_leakage.at("leakage_power_watts").get<double>(), WithinRel(5e-4));
  REQUIRE_THAT(with_leakage.at("leakage_energy_joules").get<double>(), WithinRel(1e-12));
  REQUIRE_THAT(with_leakage.at("modeled_total_energy_joules").get<double>(), WithinRel(2e-12));
  REQUIRE(energy.at("components").at(1).at("leakage_energy_joules").is_null());
  REQUIRE(energy.at("components").at(1).at("clock_period_ps") == 345);
  REQUIRE_THAT(energy.at("components").at(1).at("average_dynamic_power_watts").get<double>(), WithinRel(1.5e-3));
}

TEST_CASE("An empty interval has zero leakage energy and no average power")
{
  champsim::phase_energy_stats energy{};
  energy.components.push_back({"CPU 0", "model", "revision", "assumptions", champsim::chrono::picoseconds{250}, 0., 5e-4});
  REQUIRE(energy.leakage_energy_joules(energy.components.front()) == 0.);
  REQUIRE_FALSE(energy.average_power_watts(energy.modeled_on_chip_energy_joules()).has_value());
}

TEST_CASE("Phase energy reports a combined joule total and preserves the DRAM replay clock")
{
  using champsim::chrono::picoseconds;
  using Catch::Matchers::WithinRel;
  using time_point = champsim::chrono::clock::time_point;

  champsim::phase_stats phase;
  phase.energy = champsim::phase_energy_stats{
      time_point{picoseconds{1000}}, time_point{picoseconds{3000}}, 4,
      {{"CPU 0", "model", "revision", "assumptions", picoseconds{250}, 4e-12, 1e-3}}};
  phase.energy->dram_replay = champsim::dram_replay_energy_stats{2000, 2500, 5e-12, {5e-12}, "memspec", "cli", 0.5};

  std::ostringstream stream;
  std::vector<champsim::phase_stats> phases{phase};
  champsim::json_printer{stream}.print(phases);
  const auto report = nlohmann::json::parse(stream.str());
  const auto& energy = report.at(0).at("energy");
  const auto& replay = energy.at("dram_replay");

  CHECK_THAT(energy.at("combined_modeled_energy_joules").get<double>(), WithinRel(11e-12));
  CHECK_THAT(energy.at("modeled_average_dynamic_power_watts").get<double>(), WithinRel(2e-3));
  CHECK_THAT(energy.at("modeled_average_on_chip_power_watts").get<double>(), WithinRel(3e-3));
  CHECK_THAT(replay.at("average_power_watts").get<double>(), WithinRel(2e-3));
  CHECK(replay.at("timing_delta_ps") == 500);
  CHECK(replay.at("timing_delta_percent") == 25);
}
