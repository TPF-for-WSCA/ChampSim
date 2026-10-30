#ifndef CHAMPSIM_ITTAGE_H
#define CHAMPSIM_ITTAGE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "address.h"

// ITTAGE principles: Seznec, "A 64-Kbytes ITTAGE indirect branch predictor"
// (JWAC-2, 2011). Local full-64-bit-target organization; see docs/ittage.md.
class ittage
{
public:
  static constexpr std::size_t base_entries = 512;
  static constexpr std::size_t tagged_entries = 1024;
  static constexpr unsigned tag_bits = 11;
  static constexpr std::array<std::size_t, 6> history_lengths{4, 12, 36, 108, 324, 972};
  static constexpr uint64_t base_storage_bits = base_entries * (64 + 1);
  static constexpr uint64_t tagged_storage_bits = history_lengths.size() * tagged_entries * (64 + tag_bits + 2 + 1 + 1);
  // History bits, three folded registers per bank, ring cursor, alternate chooser,
  // and allocation-pressure counter. No prediction scratch persists between calls.
  static constexpr uint64_t history_storage_bits = history_lengths.back() + history_lengths.size() * (10 + 11 + 10) + 10 + 4 + 8;
  static constexpr uint64_t storage_bits = base_storage_bits + tagged_storage_bits + history_storage_bits;
  static_assert(storage_bits <= 64 * 1024 * 8);

  [[nodiscard]] std::pair<champsim::address, bool> prediction(champsim::address ip) const;
  void update(champsim::address ip, champsim::address target, bool taken, uint8_t branch_type);

private:
  struct base_entry {
    champsim::address target{};
    bool valid = false;
  };
  struct tagged_entry {
    champsim::address target{};
    uint16_t tag = 0;
    uint8_t confidence = 0;
    bool useful = false;
    bool valid = false;
  };
  struct lookup_result {
    std::size_t base_index{};
    std::array<std::size_t, history_lengths.size()> indices{};
    std::array<uint16_t, history_lengths.size()> tags{};
    int provider = -1;
    int alternate = -1;
    champsim::address alternate_target{};
    champsim::address target{};
  };

  std::array<base_entry, base_entries> base{};
  std::array<std::array<tagged_entry, tagged_entries>, history_lengths.size()> tables{};
  std::array<bool, history_lengths.back()> history{};
  std::size_t history_head = 0;
  std::array<uint16_t, history_lengths.size()> index_history{};
  std::array<uint16_t, history_lengths.size()> tag_history{};
  std::array<uint16_t, history_lengths.size()> alternate_tag_history{};
  int8_t use_alternate = -1;
  uint8_t allocation_pressure = 0;

  [[nodiscard]] lookup_result lookup(champsim::address ip) const;
  void train(champsim::address ip, champsim::address target);
  void push_history(bool bit);
  void update_history(champsim::address ip, champsim::address target, bool taken, uint8_t branch_type);
};

#endif
