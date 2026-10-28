#include <catch.hpp>

#include "../../../replacement/lru/lru.h"
#include "../../../replacement/srrip/srrip.h"
#include "cache.h"

TEST_CASE("Replacement storage follows policy and cache geometry")
{
  const auto num_sets = GENERATE(16u, 32u);
  CACHE cache{champsim::cache_builder{}.sets(num_sets).ways(8)};
  lru lru_policy{&cache};
  srrip srrip_policy{&cache};
  REQUIRE(lru_policy.storage_bits().has_value());
  REQUIRE(srrip_policy.storage_bits().has_value());
  CHECK(*lru_policy.storage_bits() == num_sets * 8 * 3);
  CHECK(*srrip_policy.storage_bits() == num_sets * 8 * 2);
  champsim::modules::replacement custom_policy{&cache};
  CHECK_FALSE(custom_policy.storage_bits().has_value());
}
