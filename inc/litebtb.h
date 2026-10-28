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
#include <utility>
#include <vector>

#include "address.h"
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
  };

  litebtb_hash hash_kind;
  bool two_level;
  std::size_t sets;
  std::size_t ways;
  bool clipped_tag;
  uint8_t tag_size;
  std::vector<uint8_t> target_sizes;
  std::vector<uint8_t> address_permutation;
  std::vector<entry> entries;
  std::vector<entry> l1_entries;
  std::deque<uint64_t> ras;
  uint64_t access_count{};

  [[nodiscard]] uint64_t permute(uint64_t value) const;
  [[nodiscard]] std::size_t index(uint64_t ip, std::size_t table_sets) const;
  [[nodiscard]] uint64_t tag(uint64_t ip, uint8_t capacity) const;
  [[nodiscard]] uint64_t decompress(const entry& value) const;
  entry* find(std::vector<entry>& table, std::size_t table_sets, std::size_t table_ways, uint64_t ip, bool compressed);
  void fill(std::vector<entry>& table, std::size_t table_sets, std::size_t table_ways, uint64_t ip, uint64_t target, uint8_t type,
            uint8_t required_bits, bool compressed);

public:
  litebtb_predictor(O3_CPU* cpu, litebtb_hash hash, bool enable_two_level = false);

  void initialize_btb();
  std::pair<champsim::address, bool> btb_prediction(champsim::address ip);
  void update_btb(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};

#endif
