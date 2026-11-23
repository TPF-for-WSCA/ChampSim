#ifndef CHAMPSIM_MEMORY_ENERGY_H
#define CHAMPSIM_MEMORY_ENERGY_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include "energy_stats.h"

namespace champsim
{
enum class memory_event {
  tag_lookup, victim_metadata, tag_write, data_read, data_write,
  replacement_victim, replacement_hit, replacement_writeback_hit, replacement_fill,
  mshr_compare, mshr_read, mshr_write, mshr_release, fill_compare,
  prefetch_compare, prefetch_read, prefetch_write, prefetch_remove, local_prefetch_read, local_prefetch_write,
  ubs_predictor_lookup, ubs_predictor_read, ubs_predictor_write, ubs_predictor_metadata_write, ubs_fragment_mask, ubs_candidate_metadata,
  fdip_target_compare, fdip_target_read, fdip_target_write, fdip_target_drop, fdip_target_reset,
  fdip_recent_compare, fdip_recent_write, fdip_recent_drop, fdip_request_mask,
  walk_address, walk_complete, count
};

struct memory_activity {
  std::array<uint64_t, static_cast<std::size_t>(memory_event::count)> events{};

  void record(memory_event event, std::size_t count = 1) { events[static_cast<std::size_t>(event)] += count; }
  [[nodiscard]] const uint64_t& operator[](memory_event event) const { return events[static_cast<std::size_t>(event)]; }
};

enum class request_queue { read, write, prefetch };
struct queue_activity {
  uint64_t reads = 0, writes = 0;
};

struct walk_cache_activity {
  uint64_t lookups = 0, hits = 0, fills = 0;
};

class memory_energy_model
{
public:
  virtual ~memory_energy_model() = default;
  [[nodiscard]] virtual std::vector<component_energy_stats> energy_snapshots() const = 0;
  [[nodiscard]] virtual std::vector<std::string> coverage_gaps() const = 0;
};

enum class replacement_energy_policy { unmanaged, lru, srrip, drrip };
struct cache_energy_configuration {
  bool translation;
  replacement_energy_policy replacement;
  bool fdip;
  bool has_unmodeled_prefetcher;
};

struct environment;
void configure_harcom_memory(environment& env, std::initializer_list<cache_energy_configuration> configs);
} // namespace champsim

#endif
