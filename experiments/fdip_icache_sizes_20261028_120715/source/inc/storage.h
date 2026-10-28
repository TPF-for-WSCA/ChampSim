#ifndef CHAMPSIM_STORAGE_H
#define CHAMPSIM_STORAGE_H

#include <cstdint>
#include <string>
#include <initializer_list>
#include <utility>
#include <fmt/core.h>

namespace champsim {
// ceil(log2(n)), including non-power-of-two associativities.
inline uint64_t storage_bits(uint64_t n)
{
  uint64_t bits = 0;
  for (auto value = n > 0 ? n - 1 : 0; value; value >>= 1)
    ++bits;
  return bits;
}
using storage_component = std::pair<const char*, uint64_t>;
inline void print_storage(const std::string& name, std::initializer_list<storage_component> components)
{
  uint64_t total = 0;
  for (const auto& component : components)
    total += component.second;
  fmt::print("{} STORAGE (modeled, 64-bit addresses): total={} bits ({} bytes, {:.3f} KiB)\n",
             name, total, (total + 7) / 8, static_cast<double>(total) / 8192.0);
  for (const auto& [label, bits] : components)
    fmt::print("  {}: {} bits ({} bytes, {:.3f} KiB)\n", label, bits, (bits + 7) / 8, static_cast<double>(bits) / 8192.0);
}
}
#endif
