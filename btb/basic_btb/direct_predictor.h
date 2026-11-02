#ifndef BTB_BASIC_BTB_DIRECT_PREDICTOR_H
#define BTB_BASIC_BTB_DIRECT_PREDICTOR_H

#include <cstdint>
#include <optional>

#include "address.h"
#include "champsim.h"
#include "msl/lru_table.h"

struct direct_predictor {
  enum class branch_info {
    INDIRECT,
    RETURN,
    ALWAYS_TAKEN,
    CONDITIONAL,
  };

  const std::size_t sets;
  const std::size_t ways;
  const uint8_t tag_bits;

  struct btb_entry_t {
    champsim::address ip_tag{};
    champsim::address target{};
    branch_info type = branch_info::ALWAYS_TAKEN;
    bool is_call = false;

    auto index() const
    {
      using namespace champsim::data::data_literals;
      return ip_tag.slice_upper<2_b>();
    }
    auto tag() const
    {
      using namespace champsim::data::data_literals;
      return ip_tag.slice_upper<2_b>();
    }
  };

private:
  const uint64_t ip_mask;

public:
  champsim::msl::lru_table<btb_entry_t> BTB;
  explicit direct_predictor(std::size_t configured_sets = 1024, std::size_t configured_ways = 8,
                            std::optional<uint8_t> configured_tag_bits = std::nullopt);
  std::optional<btb_entry_t> check_hit(champsim::address ip, bool update_lru = true);
  void update(champsim::address ip, champsim::address branch_target, uint8_t branch_type);
};

#endif
