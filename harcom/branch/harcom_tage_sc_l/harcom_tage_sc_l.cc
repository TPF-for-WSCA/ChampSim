#include "harcom_tage_sc_l.h"

#include "../../bridge.h"

namespace
{
struct access_cost {
  double read;
  double write;
};

template <std::size_t Depth, std::size_t Width>
constexpr access_cost sram_cost{hcm::sram<Depth, Width>::EREAD, hcm::sram<Depth, Width>::EWRITE};
} // namespace

harcom_tage_sc_l::harcom_tage_sc_l(O3_CPU* cpu) : tage_sc_l(cpu) { track_table_activity(); }

champsim::energy_model_snapshot harcom_tage_sc_l::energy_snapshot() const
{
  const auto& activity = observed_table_activity();
  double energy = 0;
  const auto accumulate = [&](table location, access_cost cost) {
    const auto& counts = activity.at(static_cast<std::size_t>(location));
    energy += static_cast<double>(counts.reads) * cost.read + static_cast<double>(counts.writes) * cost.write;
  };

  accumulate(table::bimodal_prediction, sram_cost<8192, 1>);
  accumulate(table::bimodal_hysteresis, sram_cost<2048, 1>);
  accumulate(table::short_tagged, sram_cost<1024, 12>); // 8-bit tag, direction counter and useful bit.
  accumulate(table::long_tagged, sram_cost<1024, 16>);
  accumulate(table::loop, sram_cost<8, 39>); // Four independently addressed ways.
  accumulate(table::sc_128, sram_cost<128, 6>);
  accumulate(table::sc_256, sram_cost<256, 6>);
  accumulate(table::sc_512, sram_cost<512, 6>);
  accumulate(table::sc_1024, sram_cost<1024, 6>);
  accumulate(table::sc_weight, sram_cost<8, 6>);
  accumulate(table::sc_threshold, sram_cost<64, 8>);
  accumulate(table::sc_local1, sram_cost<256, 11>);
  accumulate(table::sc_local2, sram_cost<16, 16>);
  accumulate(table::sc_local3, sram_cost<16, 9>);
  accumulate(table::sc_imli, sram_cost<256, 10>);
  accumulate(table::alternate_chooser, sram_cost<16, 5>);

  return {"HARCOM TAGE-SC-L table accesses", "5ceea8358ddbab94e5621e2536530b860cc1e5b7",
          "SRAM access energy for parallel tagged and loop banks, statistical corrector tables and local histories. "
          "Parallel rows are reused within a lookup. Logic, global and folded history registers, and bulk useful-bit aging are unmodeled.",
          energy};
}
