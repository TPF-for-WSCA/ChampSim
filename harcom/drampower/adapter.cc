#include "dram_power_model.h"
#include "scheduler.h"

#include <cmath>
#include <fstream>
#include <stdexcept>
#include <utility>

#include <DRAMPower/standards/ddr4/DDR4.h>
#include <DRAMPower/standards/ddr5/DDR5.h>
#include <DRAMUtils/memspec/MemSpec.h>
#include <nlohmann/json.hpp>

namespace champsim
{
namespace
{
using drampower::cycle;
using drampower::operation;
using json = nlohmann::json;

constexpr auto revision = "8b0dedaf45f9bf528792561f87786aa827db94cc";

struct profile {
  double tick_ps;
  drampower::timing timings;
};

profile read_profile(const json& memspec, dram_power_geometry g)
{
  const auto type = memspec.at("memoryType").get<std::string>();
  if (type != "DDR4" && type != "DDR5")
    throw std::invalid_argument("linked replay supports DDR4 and DDR5");
  const auto& arch = memspec.at("memarchitecturespec");
  const auto& t = memspec.at("memtimingspec");
  const auto check = [](const char* field, std::size_t spec, std::size_t simulation) {
    if (spec != simulation)
      throw std::invalid_argument(std::string("DRAMPower ") + field + " differs from ChampSim");
  };
  check("ranks", arch.at("nbrOfRanks"), g.ranks);
  check("bank groups", arch.at("nbrOfBankGroups"), g.groups);
  check("banks", arch.at("nbrOfBanks"), g.groups * g.banks_per_group);
  check("rows", arch.at("nbrOfRows"), g.rows);
  check("columns", arch.at("nbrOfColumns"), g.columns);
  check("channel width", arch.at("width").get<std::size_t>() * arch.at("nbrOfDevices").get<std::size_t>(), g.width_bits);
  check("burst length", arch.at("burstLength"), g.burst_length);
  if (arch.at("RefMode") != 1 || arch.at("dataRate") != 2 || t.value("AL", 0) != 0)
    throw std::invalid_argument("linked replay requires 1x all-bank refresh, DDR signaling, and zero additive latency");

  const auto tick = t.at("tCK").get<double>() * 1e12;
  if (!std::isfinite(tick) || tick <= 0 || std::abs(tick - static_cast<double>(g.clock.count())) >= 1
      || std::abs(tick / 2 - static_cast<double>(g.beat.count())) >= 1)
    throw std::invalid_argument("DRAMPower command or data clock differs from ChampSim");

  const auto field = [&](const char* name) { return t.value(name, cycle{0}); };
  const auto largest = [&](std::initializer_list<std::string_view> prefixes) {
    cycle result = 0;
    for (auto entry = t.begin(); entry != t.end(); ++entry)
      for (auto prefix : prefixes)
        if (entry.key().compare(0, prefix.size(), prefix) == 0)
          result = std::max(result, entry.value().get<cycle>());
    return result;
  };
  auto refi = static_cast<cycle>(std::floor(static_cast<double>(g.refresh_interval.count()) / tick));
  if (const auto device_refi = type == "DDR4" ? field("REFI") : field("REFI1"); device_refi != 0)
    refi = std::min(refi, device_refi);
  const auto rfc = largest({"RFC1"});
  const auto rp = t.at("RP").get<cycle>();
  const auto ras = t.at("RAS").get<cycle>();
  return {tick,
          {type == "DDR5" ? cycle{2} : cycle{1}, t.at("RCD").get<cycle>(), ras,
           std::max(field("RC"), ras + rp), rp, field("PPD"), largest({"RRD_S"}), largest({"RRD_L"}), field("RRD_dlr"),
           largest({"FAW"}), largest({"CCD_S"}), largest({"CCD_L", "CCD_M"}), largest({"CCD_dlr", "CCD_WR_dlr", "CCD_WR_dpr"}),
           t.at("RTP").get<cycle>(), t.at("WR").get<cycle>(), field("WTR_S"), largest({"WTR_L", "WTR_M"}),
           std::max(cycle{2}, field("RTRS")), t.at("RL").get<cycle>(), t.at("WL").get<cycle>(),
           (g.burst_length + 1) / 2, std::max(field("RPRE"), field("WPRE")),
           std::max(field("RPST"), field("WPST")), rfc, refi}};
}

DRAMPower::CmdType command_type(operation op)
{
  using DRAMPower::CmdType;
  switch (op) {
  case operation::activate: return CmdType::ACT;
  case operation::precharge: return CmdType::PRE;
  case operation::precharge_all: return CmdType::PREA;
  case operation::read: return CmdType::RD;
  case operation::write: return CmdType::WR;
  case operation::refresh: return CmdType::REFA;
  }
  throw std::logic_error("invalid DRAM operation");
}

using dram = DRAMPower::dram_base<DRAMPower::CmdType>;

template <typename Spec, typename Standard>
std::unique_ptr<dram> make_device(const DRAMUtils::MemSpec::MemSpecVariant& spec, const DRAMPower::config::SimConfig& config)
{
  return std::make_unique<Standard>(Spec::from_memspec(spec), config);
}

class linked_model final : public dram_power_model
{
  const dram_power_geometry geometry;
  const std::string memspec_file;
  const double toggle;
  profile device;
  DRAMUtils::MemSpec::MemSpecVariant memspec;
  std::string standard;
  chrono::clock::time_point start{};
  std::vector<dram_power_burst> bursts;

public:
  linked_model(dram_power_geometry shape, const std::string& path, double rate) : geometry(shape), memspec_file(path), toggle(rate)
  {
    std::ifstream input{path};
    auto root = json::parse(input);
    const auto& definition = root.at("memspec");
    device = read_profile(definition, shape);
    standard = definition.at("memoryType").get<std::string>();
    auto spec = DRAMUtils::parse_memspec_from_json(root);
    if (!spec)
      throw std::invalid_argument("DRAMPower could not parse the device specification");
    memspec = std::move(*spec);
  }

  void begin(chrono::clock::time_point time) override
  {
    start = time;
    bursts.clear();
  }

  void observe(dram_power_burst burst) override { bursts.push_back(burst); }

  dram_replay_energy_stats finish(chrono::clock::time_point end) override
  {
    const auto cycles_for = [this](chrono::picoseconds duration) {
      return static_cast<cycle>(std::ceil(static_cast<double>(std::max(duration.count(), chrono::picoseconds::rep{0})) / device.tick_ps));
    };
    std::vector<drampower::scheduler> schedulers;
    schedulers.reserve(geometry.channels);
    for (std::size_t i = 0; i < geometry.channels; ++i)
      schedulers.emplace_back(device.timings, geometry.ranks, geometry.groups, geometry.banks_per_group);
    for (const auto& burst : bursts) {
      if (burst.data_start < start || burst.data_start >= end)
        continue;
      schedulers.at(burst.channel).add({cycles_for(burst.reserved - start), cycles_for(burst.data_start - start),
                                       {burst.rank, burst.group, burst.bank, burst.row, burst.column}, burst.write});
    }

    const auto native_cycles = cycles_for(end - start);
    auto horizon = native_cycles;
    for (auto& schedule : schedulers)
      horizon = std::max(horizon, schedule.schedule(native_cycles));

    const auto idle = DRAMUtils::Config::TogglingRateIdlePattern::L;
    DRAMPower::config::SimConfig config;
    config.toggleRateDefinition = DRAMUtils::Config::ToggleRateDefinition{toggle, toggle, toggle, toggle, idle, idle};
    dram_replay_energy_stats result{static_cast<double>((end - start).count()), static_cast<double>(horizon) * device.tick_ps,
                                    0, {}, {}, {}, toggle, "DRAMPower 6.0 linked bank-parallel replay", revision, memspec_file};
    drampower::burst_delays total_delay;
    for (auto& schedule : schedulers) {
      const auto& delay = schedule.burst_start_delays();
      total_delay.count += delay.count;
      total_delay.sum += delay.sum;
      total_delay.max = std::max(total_delay.max, delay.max);
      auto channel = standard == "DDR4" ? make_device<DRAMPower::MemSpecDDR4, DRAMPower::DDR4>(memspec, config)
                                        : make_device<DRAMPower::MemSpecDDR5, DRAMPower::DDR5>(memspec, config);
      for (auto command : schedule.finish(horizon)) {
        const auto& a = command.address;
        channel->doCommand({command.when, command_type(command.op), {a.bank, a.group, a.rank, a.row, a.column}});
      }
      channel->doCommand({horizon, DRAMPower::CmdType::END_OF_SIMULATION});
      const auto energy = channel->calcCoreEnergy(horizon).total() + channel->calcInterfaceEnergy(horizon).total();
      if (!std::isfinite(energy) || energy < 0)
        throw std::runtime_error("linked DRAMPower produced invalid energy");
      result.channel_energy_joules.push_back(energy);
      result.energy_joules += energy;
    }
    if (total_delay.count != 0)
      result.burst_delay = dram_replay_burst_delay{total_delay.count, static_cast<double>(total_delay.sum / total_delay.count) * device.tick_ps,
                                                  static_cast<double>(total_delay.max) * device.tick_ps};
    return result;
  }
};
} // namespace

std::unique_ptr<dram_power_model> make_linked_drampower(dram_power_geometry geometry, const std::string& memspec, double toggle)
{
  return std::make_unique<linked_model>(geometry, memspec, toggle);
}
} // namespace champsim
