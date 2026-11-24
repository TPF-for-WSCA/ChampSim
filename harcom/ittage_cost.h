#ifndef CHAMPSIM_HARCOM_ITTAGE_COST_H
#define CHAMPSIM_HARCOM_ITTAGE_COST_H

#include "leakage.h"
#include "ittage.h"

namespace champsim::harcom
{
constexpr auto target_select = hcm::MUX<2, 64>;
constexpr auto ittage_banks = ittage::history_lengths.size();
constexpr auto index_bits = champsim::lg2(ittage::tagged_entries);
using indirect_base = hcm::sram<ittage::base_entries, 65>;
using indirect_tagged = hcm::sram<ittage::tagged_entries, 64 + ittage::tag_bits + 2 + 1>;
constexpr auto indirect_mux = hcm::MUX<ittage_banks + 1, 64>;
constexpr auto useful_mux = hcm::MUX<ittage::tagged_entries, 1>;
constexpr auto indirect_fold_bits = ittage_banks * (index_bits * 2 + ittage::tag_bits);
constexpr auto indirect_lookup = indirect_base::EREAD
    + ittage_banks * (indirect_tagged::EREAD + hcm::EQUAL<ittage::tag_bits>.e + hcm::AND<2>.e + 2 * (index_bits + ittage::tag_bits) * hcm::XOR<2>.e)
    + 18 * hcm::XOR<2>.e + 2 * hcm::priority_encoder<ittage_banks>.e + 2 * (indirect_mux[0].e + indirect_mux[1].e)
    + hcm::NEQ<64>.e + target_select[0].e + target_select[1].e;
constexpr auto indirect_history_write = hcm::flipflops<ittage::history_lengths.back()>::write_energy_fJ
    + hcm::flipflops<indirect_fold_bits>::write_energy_fJ + ittage_banks * 6 * hcm::XOR<2>.e;

constexpr double ittage_leakage_watts()
{
  const auto tagged_logic = hcm::EQUAL<ittage::tag_bits> || hcm::AND<2>
      || hcm::XOR<2> * (2 * (index_bits + ittage::tag_bits) + 6)
      || useful_mux[0] || useful_mux[1] || hcm::decoder<ittage::tagged_entries>;
  const auto common_logic = hcm::XOR<2> * 58 || hcm::priority_encoder<ittage_banks> * 2
      || (indirect_mux[0] || indirect_mux[1]) * 2 || hcm::NEQ<64> || target_select[0] || target_select[1]
      || hcm::EQUAL<64> * 3 || hcm::INC<4> || hcm::INC<8>;
  return (indirect_base::leakage_mW() + ittage_banks * indirect_tagged::leakage_mW()) * 1e-3
      + register_leakage_watts(ittage_banks * ittage::tagged_entries + ittage::history_lengths.back() + indirect_fold_bits + 4 + 8)
      + logic_leakage_watts(tagged_logic, ittage_banks) + logic_leakage_watts(common_logic);
}

inline double ittage_energy(frontend_event event, std::size_t count)
{
  using champsim::frontend_event;
  switch (event) {
  case frontend_event::ittage_lookup:
    return indirect_lookup;
  case frontend_event::ittage_train:
    return 3 * hcm::EQUAL<64>.e;
  case frontend_event::ittage_tagged_write:
    return indirect_tagged::EWRITE;
  case frontend_event::ittage_base_write:
    return indirect_base::EWRITE;
  case frontend_event::ittage_useful_read:
    return useful_mux[0].e + useful_mux[1].e;
  case frontend_event::ittage_useful_write:
    return hcm::decoder<ittage::tagged_entries>.e + hcm::flipflops<1>::write_energy_fJ;
  case frontend_event::ittage_useful_reset:
    return ittage_banks * hcm::flipflops<ittage::tagged_entries>::write_energy_fJ;
  case frontend_event::ittage_chooser_write:
    return hcm::INC<4>.e + hcm::flipflops<4>::write_energy_fJ;
  case frontend_event::ittage_pressure_write:
    return hcm::INC<8>.e + hcm::flipflops<8>::write_energy_fJ;
  case frontend_event::ittage_history_mix:
    return 40 * hcm::XOR<2>.e;
  case frontend_event::ittage_history_bit:
    return static_cast<double>(count) * indirect_history_write;
  default:
    return 0;
  }
}
} // namespace champsim::harcom

#endif
