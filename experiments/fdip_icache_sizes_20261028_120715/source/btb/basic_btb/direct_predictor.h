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

  struct btb_entry_t {
    champsim::address ip_tag{};
    champsim::address target{};
    branch_info type = branch_info::ALWAYS_TAKEN;

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

  champsim::msl::lru_table<btb_entry_t> BTB;
  explicit direct_predictor(std::size_t configured_sets = 1024, std::size_t configured_ways = 8)
      : sets(configured_sets), ways(configured_ways), BTB(sets, ways)
  {
    if (ways == 0)
      throw std::invalid_argument("Basic BTB ways must be nonzero");
  }
  std::optional<btb_entry_t> check_hit(champsim::address ip);
  void update(champsim::address ip, champsim::address branch_target, uint8_t branch_type);
};

#endif
