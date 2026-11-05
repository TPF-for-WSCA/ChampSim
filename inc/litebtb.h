/*
 * LiteBTB, named XBTBX in the MICRO'26 artifact.
 *
 * This file contains the common implementation interface used by the plain,
 * hashed, and two-level variants.  Module-specific directories only select
 * the tag hash and whether the 64-set, 8-way L1 BTB is enabled.
 */
#ifndef LITEBTB_H
#define LITEBTB_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

#include "address.h"
#include "btb_prediction_token.h"
#include "ittage.h"
#include "modules.h"

enum class litebtb_hash { identity, half_xor, full_xor, geometric, pearson, rabin, sha3, zobrist };

class litebtb_predictor : public champsim::modules::btb
{
  enum class branch_info : uint8_t { indirect, return_branch, always_taken, conditional };

  struct entry {
    uint64_t ip{};
    uint64_t target{};
    uint64_t last_used{};
    uint8_t target_size{64};
    uint8_t precise_type{};
    branch_info type{branch_info::always_taken};
    std::optional<std::size_t> utb_pointer;
    uint64_t utb_generation{};
    uint64_t generation{};
  };

  struct utb_entry {
    uint64_t tag{};
    uint64_t last_used{};
    std::size_t references{};
    uint64_t generation{};
  };

  litebtb_hash hash_kind;
  bool two_level;
  std::size_t sets;
  std::size_t ways;
  bool clipped_tag;
  uint8_t tag_size;
  std::optional<uint8_t> full_target_tag_size;
  std::vector<uint8_t> target_sizes;
  std::vector<uint8_t> address_permutation;
  std::vector<entry> entries;
  std::vector<entry> l1_entries;
  std::vector<utb_entry> utb;
  std::size_t utb_ways{};
  bool utb_reference_count{};
  uint64_t next_generation{};
  std::deque<uint64_t> ras;
  ittage indirect{};
  uint64_t access_count{};

  [[nodiscard]] uint64_t permute(uint64_t value) const;
  [[nodiscard]] std::size_t index(uint64_t ip, std::size_t table_sets) const;
  [[nodiscard]] uint8_t tag_bits(uint8_t capacity) const;
  [[nodiscard]] uint64_t tag(uint64_t ip, uint8_t capacity) const;
  [[nodiscard]] uint64_t decompress(const entry& value) const;
  [[nodiscard]] bool uses_utb(uint8_t capacity) const;
  [[nodiscard]] std::optional<std::size_t> find_utb(uint64_t ip, uint8_t capacity) const;
  std::size_t allocate_utb(uint64_t ip, uint8_t capacity);
  [[nodiscard]] bool matches(const entry& value, uint64_t ip, bool compressed, std::optional<std::size_t> pointer) const;
  void invalidate(entry& value);
  entry* find(std::vector<entry>& table, std::size_t table_sets, std::size_t table_ways, uint64_t ip, bool compressed);
  void fill(std::vector<entry>& table, std::size_t table_sets, std::size_t table_ways, uint64_t ip, uint64_t target, uint8_t type,
            uint8_t required_bits, bool compressed);

public:
  litebtb_predictor(O3_CPU* cpu, litebtb_hash hash, bool enable_two_level = false);

  void initialize_btb();
  std::pair<champsim::address, bool> btb_prediction(champsim::address ip);
  void update_btb(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
  void btb_decode(champsim::btb_prediction_token token, uint8_t branch_type);

  // Exposes replacement state for experiment diagnostics and invariant checks.
  [[nodiscard]] std::vector<std::size_t> utb_reference_counts() const;
};

#endif
