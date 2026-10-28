#ifndef REPLACEMENT_SHIP_H
#define REPLACEMENT_SHIP_H

#include <array>
#include <vector>

#include "cache.h"
#include "modules.h"
#include "storage.h"
#include "msl/bits.h"
#include "msl/stat_methods.h"

struct ship : public champsim::modules::replacement {
private:
  int& get_rrpv(long set, long way);

public:
  static constexpr int maxRRPV = 3;
  static constexpr std::size_t SHCT_SIZE = 16384;
  static constexpr unsigned SHCT_PRIME = 16381;
  static constexpr unsigned SHCT_MAX = 7;

  // sampler structure
  class SAMPLER_class
  {
  public:
    bool valid = false;
    bool used = false;
    champsim::address address{};
    champsim::address ip{};
    uint64_t last_used = 0;
  };

  long NUM_SET, NUM_WAY;
  uint64_t access_count = 0;

  // sampler
  std::vector<SAMPLER_class> sampler;
  std::vector<int> rrpv_values;
  champsim::msl::categorizer<long> set_categorizer;
  champsim::data::bits sampler_tag_bits;

  // prediction table structure
  std::vector<std::array<champsim::msl::fwcounter<champsim::msl::lg2(SHCT_MAX + 1)>, SHCT_SIZE>> SHCT;

  explicit ship(CACHE* cache);

  std::optional<uint64_t> storage_bits() const
  {
    // Sampled tag + signature + valid/used + per-set LRU rank, and 3-bit SHCT counters.
    const uint64_t sampler_bits = 64 - champsim::to_underlying(sampler_tag_bits) + 64 + 2 + champsim::storage_bits(static_cast<uint64_t>(NUM_WAY));
    return rrpv_values.size() * champsim::storage_bits(maxRRPV + 1)
        + sampler.size() * sampler_bits + SHCT.size() * SHCT_SIZE * champsim::storage_bits(SHCT_MAX + 1);
  }

  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type);
  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type);
  void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, uint8_t hit);

  // use this function to print out your own stats at the end of simulation
  // void replacement_final_stats() {}
};

#endif
