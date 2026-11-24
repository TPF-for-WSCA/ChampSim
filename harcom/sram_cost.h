#ifndef CHAMPSIM_HARCOM_SRAM_COST_H
#define CHAMPSIM_HARCOM_SRAM_COST_H

#include <algorithm>
#include <bit>

#include "leakage.h"

namespace champsim::harcom
{
struct access_cost {
  double read = 0;
  double write = 0;
  double leakage_watts = 0;
};

template <std::size_t Rows>
access_cost sram_slices(std::size_t bits)
{
  using narrow = hcm::sram_bank<Rows, 4, 4>;
  using medium = hcm::sram_bank<Rows, 8, 8>;
  using wide = hcm::sram_bank<Rows, 16, 16>;
  if (bits <= 4)
    return {narrow::EREAD, narrow::EWRITE, narrow::leakage_mW() * 1e-3};
  if (bits <= 8)
    return {medium::EREAD, medium::EWRITE, medium::leakage_mW() * 1e-3};
  const auto slices = static_cast<double>((bits + 15) / 16);
  return {slices * wide::EREAD, slices * wide::EWRITE, slices * wide::leakage_mW() * 1e-3};
}

inline access_cost sram_row(std::size_t rows, std::size_t bits)
{
  if (rows == 0 || bits == 0)
    return {};
  if (rows <= 8)
    return sram_slices<8>(bits);
  if (rows <= 16)
    return sram_slices<16>(bits);
  if (rows <= 32)
    return sram_slices<32>(bits);
  if (rows <= 64)
    return sram_slices<64>(bits);

  // A high address decode activates one 64-row bank. The model covers local
  // SRAM wiring and decode logic. Global bank wiring needs a floorplan.
  const auto banks = (rows - 1) / 64 + 1;
  const auto select = hcm::anding(std::bit_width(banks - 1), hcm::OUTCAP);
  auto local = sram_slices<64>(bits);
  return {local.read + select.e, local.write + select.e,
          static_cast<double>(banks) * local.leakage_watts + logic_leakage_watts(select, banks)};
}

inline double match_energy(std::size_t bits)
{
  const auto reduction = hcm::anding(std::max<std::size_t>(bits, 1), hcm::OUTCAP);
  return static_cast<double>(bits) * hcm::xnor2(reduction.ci).e + reduction.e;
}

inline double match_leakage_watts(std::size_t bits, std::size_t replicas = 1)
{
  const auto reduction = hcm::anding(std::max<std::size_t>(bits, 1), hcm::OUTCAP);
  return logic_leakage_watts(hcm::xnor2(reduction.ci), bits * replicas) + logic_leakage_watts(reduction, replicas);
}

} // namespace champsim::harcom

#endif
