#ifndef CHAMPSIM_HARCOM_LEAKAGE_H
#define CHAMPSIM_HARCOM_LEAKAGE_H

#include "bridge.h"

namespace champsim::harcom
{
constexpr double logic_leakage_watts(const hcm::circuit& circuit, std::size_t replicas = 1)
{
  return hcm::leakage_power_mW(replicas * circuit.f, 0) * 1e-3;
}

constexpr double register_leakage_watts(std::size_t bits)
{
  return hcm::leakage_power_mW(bits * hcm::flipflops<1>::fins, 0) * 1e-3;
}
} // namespace champsim::harcom

#endif
