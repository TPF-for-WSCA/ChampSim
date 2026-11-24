#ifndef CHAMPSIM_BACKEND_ENERGY_H
#define CHAMPSIM_BACKEND_ENERGY_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include "energy_stats.h"

namespace champsim
{
enum class backend_event {
  fetch_in, fetch_out, fetch_ready, fetch_release,
  decode_in, decode_out, decode_release, dib_in, dib_out, dib_release,
  dispatch_in, dispatch_out, dispatch_release,
  rob_in, rob_rename, rob_issue, rob_complete, rob_retire, rob_release, rob_memory_done, recover,
  rat_read, rat_write, committed_read, committed_write, free_take, free_put, rename_metadata_read, rename_metadata_write,
  checkpoint, restore, select_entry, ready_read, ready_write, wake, operand_read, operand_write,
  load_in, load_ready, load_issue, load_send, load_release, load_match, store_in, store_ready, store_execute, store_send,
  store_release, store_match, forward_wait, forward_data, integer_execute, branch_execute, address_execute, cycle, count
};

struct backend_activity {
  std::array<uint64_t, static_cast<std::size_t>(backend_event::count)> events{};
  void record(backend_event event, std::size_t count = 1) { events[static_cast<std::size_t>(event)] += count; }
  [[nodiscard]] uint64_t operator[](backend_event event) const { return events[static_cast<std::size_t>(event)]; }
};

enum class link_payload { cache_line, translation, page_table_entry };

class channel;
struct environment;
struct backend_link_configuration {
  channel* connection;
  std::string_view name;
  chrono::picoseconds clock_period;
  link_payload payload;
  bool byte_mask;
};

class backend_energy_model
{
public:
  virtual ~backend_energy_model() = default;
  [[nodiscard]] virtual std::vector<component_energy_stats> energy_snapshots() const = 0;
  [[nodiscard]] virtual std::vector<std::string> coverage_gaps() const = 0;
};

void configure_harcom_backend(environment& env, double link_length_um, std::initializer_list<backend_link_configuration> links);
} // namespace champsim

#endif
