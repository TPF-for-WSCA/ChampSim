#include "harcom_tage_sc_l.h"

#include "../../leakage.h"

namespace
{
using champsim::harcom::logic_leakage_watts;
using champsim::harcom::register_leakage_watts;

struct access_cost {
  double read;
  double write;
};

template <std::size_t Depth, std::size_t Width>
constexpr access_cost sram_cost{hcm::sram<Depth, Width>::EREAD, hcm::sram<Depth, Width>::EWRITE};

template <std::size_t Inputs, std::size_t Bits>
constexpr double mux_cost = hcm::MUX<Inputs, Bits>[0].e + hcm::MUX<Inputs, Bits>[1].e;

constexpr double xor_bit = hcm::XOR<2>.e;
// For modulo 10 and 20, low bits are wires and the remaining bits reduce
// modulo five. Nibble folding works because sixteen is one modulo five.
constexpr double mod_five = hcm::ADDN<4, 16>.e + hcm::ADD<5>.e + 6 * hcm::GTE<5>.e + mux_cost<6, 3>;
constexpr double tag_selection = 2 * mod_five + 21 * (hcm::ADD<5>.e + hcm::GTE<5>.e + hcm::SUB<5>.e)
    + 6 * hcm::EQUAL<8>.e + 15 * hcm::EQUAL<12>.e + 2 * hcm::priority_encoder<21>.e
    + 2 * mux_cost<21, 3> + 4 * hcm::EQUAL<3>.e + mux_cost<2, 1>;
// Physical banks are shared across history lengths. GEHL's shortest tables
// use only half their allocated C++ rows. The useful planes reset in parallel.
constexpr double leakage_watts()
{
  constexpr auto short_banks = 10;
  constexpr auto long_banks = 20;
  constexpr auto tagged_banks = short_banks + long_banks;
  constexpr auto sc_256_banks = 3 + 2 + 2 + 2; // Bias, path, second local and IMLI history.
  constexpr auto sc_512_banks = 2 + 1 + 2 + 1 + 2; // Global, path and all three local groups.
  constexpr auto sram_mW = hcm::sram<8192, 1>::leakage_mW() + hcm::sram<2048, 1>::leakage_mW()
      + short_banks * hcm::sram<1024, 11>::leakage_mW() + long_banks * hcm::sram<1024, 15>::leakage_mW()
      + 4 * hcm::sram<8, 39>::leakage_mW()
      + hcm::sram<128, 6>::leakage_mW() + sc_256_banks * hcm::sram<256, 6>::leakage_mW()
      + sc_512_banks * hcm::sram<512, 6>::leakage_mW() + 2 * hcm::sram<1024, 6>::leakage_mW()
      + 8 * hcm::sram<8, 6>::leakage_mW() + hcm::sram<64, 8>::leakage_mW()
      + hcm::sram<256, 11>::leakage_mW() + hcm::sram<16, 16>::leakage_mW()
      + hcm::sram<16, 9>::leakage_mW() + hcm::sram<256, 10>::leakage_mW() + hcm::sram<16, 5>::leakage_mW();
  constexpr auto useful_mux = hcm::MUX<1024, 1>;
  constexpr auto bank_logic = useful_mux[0] || useful_mux[1] || hcm::decoder<1024>;
  constexpr auto selection = hcm::EQUAL<8> * 6 || hcm::EQUAL<12> * 15 || hcm::priority_encoder<21> * 2;
  constexpr auto sc_logic = hcm::ADDN<7, 3> || hcm::ADDN<10, 8> || hcm::ADD<12> || hcm::ADDN<1, 7> || hcm::GTE<12> * 4;
  constexpr auto history = 3000 + 1092 + 27 + 40 + 8;
  constexpr auto update_and_reuse = 12 + 11 + 32 + 66;
  return sram_mW * 1e-3 + register_leakage_watts(tagged_banks * 1024 + history + update_and_reuse)
      + logic_leakage_watts(bank_logic, tagged_banks) + logic_leakage_watts(selection || sc_logic || hcm::EQUAL<64>)
      + logic_leakage_watts(hcm::XOR<2>, 36 * 3 * 3 + 7 + 16);
}

} // namespace

harcom_tage_sc_l::harcom_tage_sc_l(O3_CPU* cpu) : tage_sc_l(cpu) { track_activity(); }

champsim::energy_model_snapshot harcom_tage_sc_l::energy_snapshot() const
{
  const auto& activity = observed_table_activity();
  const auto& logic_events = observed_logic_activity();
  double energy = 0;
  const auto accumulate = [&](table location, access_cost cost) {
    const auto& counts = activity.at(static_cast<std::size_t>(location));
    energy += static_cast<double>(counts.reads) * cost.read + static_cast<double>(counts.writes) * cost.write;
  };
  const auto accumulate_logic = [&](logic operation, double cost) {
    energy += static_cast<double>(logic_events.at(static_cast<std::size_t>(operation))) * cost;
  };

  accumulate(table::bimodal_prediction, sram_cost<8192, 1>);
  accumulate(table::bimodal_hysteresis, sram_cost<2048, 1>);
  accumulate(table::short_tagged, sram_cost<1024, 11>); // 8-bit tag and 3-bit direction counter.
  accumulate(table::long_tagged, sram_cost<1024, 15>);
  accumulate(table::tagged_useful, {mux_cost<1024, 1>, hcm::decoder<1024>.e + hcm::flipflops<1>::write_energy_fJ});
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

  accumulate_logic(logic::reuse_check, hcm::EQUAL<64>.e + hcm::AND<2>.e);
  accumulate_logic(logic::reuse_write, hcm::flipflops<66>::write_energy_fJ);
  accumulate_logic(logic::reuse_invalidate, hcm::flipflops<1>::write_energy_fJ);
  accumulate_logic(logic::hash_xor_bits, xor_bit);
  accumulate_logic(logic::tagged_selection, tag_selection);
  accumulate_logic(logic::loop_prediction, 25 * xor_bit + 4 * hcm::EQUAL<10>.e + hcm::priority_encoder<4>.e + mux_cost<4, 29>
                                            + hcm::IMUL<4, 10>.e + hcm::GT<14>.e + hcm::INC<10>.e + hcm::EQUAL<10>.e);
  accumulate_logic(logic::loop_training, hcm::INC<10>.e + hcm::EQUAL<10>.e + hcm::GT<10>.e + 2 * hcm::INC<4>.e);
  accumulate_logic(logic::sc_prediction, hcm::ADDN<7, 3>.e + 8 * mux_cost<2, 9> + hcm::ADDN<10, 8>.e
                                          + hcm::ADD<12>.e + hcm::ADDN<1, 7>.e + 4 * hcm::GTE<12>.e + mux_cost<4, 1>);
  accumulate_logic(logic::sc_training, hcm::INC<8>.e + hcm::INC<12>.e + 2 * hcm::GTE<8>.e + 2 * hcm::GTE<12>.e
                                        + 8 * (hcm::SUB<12>.e + hcm::ADD<12>.e) + hcm::flipflops<12>::write_energy_fJ);
  accumulate_logic(logic::sc_group_add, hcm::ADD<9>.e);
  accumulate_logic(logic::counter_bits, hcm::XOR<2>.e + hcm::AND<2>.e + mux_cost<2, 1>);
  accumulate_logic(logic::allocation, hcm::priority_encoder<21>.e + hcm::ADD<11>.e + hcm::EQUAL<11>.e + hcm::flipflops<11>::write_energy_fJ);
  accumulate_logic(logic::random_update, hcm::INC<32>.e + 2 * hcm::ADD<32>.e + 64 * xor_bit + hcm::flipflops<32>::write_energy_fJ);
  // Fixed taps model the live history window. Two or three incoming bits
  // are combined before one write of the history and fold registers.
  accumulate_logic(logic::history_shift, hcm::flipflops<3000>::write_energy_fJ + hcm::flipflops<1092>::write_energy_fJ
                                         + hcm::flipflops<27>::write_energy_fJ + 16 * xor_bit);
  accumulate_logic(logic::history_fold, (36 * 3 * 3 + 7) * xor_bit);
  accumulate_logic(logic::sc_history, hcm::GTE<64>.e + hcm::flipflops<40>::write_energy_fJ
                                    + hcm::INC<8>.e + hcm::flipflops<8>::write_energy_fJ);
  accumulate_logic(logic::useful_reset_banks, hcm::flipflops<1024>::write_energy_fJ);

  return {"HARCOM TAGE-SC-L", "5ceea8358ddbab94e5621e2536530b860cc1e5b7",
          "Parallel SRAM banks, modeled hash/selection and training logic, shift-register global and folded histories, "
          "and resettable useful-bit planes. Datapath energy is analytical. Global wiring and clock distribution are unmodeled.",
          energy, leakage_watts()};
}
