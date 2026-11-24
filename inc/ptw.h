/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PTW_H
#define PTW_H

#include <array>
#include <deque>
#include <limits>   // for numeric_limits
#include <optional> // for optional
#include <string>

#include "address.h"
#include "bandwidth.h"
#include "channel.h"
#include "operable.h"
#include "memory_energy.h"
#include "ptw_builder.h"
#include "util/lru_table.h"
#include "waitable.h"

class VirtualMemory;
class PageTableWalker : public champsim::operable
{
  struct pscl_entry {
    champsim::address vaddr;
    champsim::address ptw_addr;
    std::size_t level;
  };

  struct pscl_indexer {
    champsim::data::bits shamt;
    auto operator()(const pscl_entry& entry) const { return entry.vaddr.slice_upper(shamt); }
  };

  using pscl_type = champsim::lru_table<pscl_entry, pscl_indexer, pscl_indexer>;
  using channel_type = champsim::channel;
  using request_type = typename channel_type::request_type;
  using response_type = typename channel_type::response_type;

  struct mshr_type {
    champsim::address address{};
    champsim::address v_address{};
    champsim::waitable<champsim::address> data{};

    std::vector<uint64_t> instr_depend_on_me{};
    std::vector<std::deque<response_type>*> to_return{};

    uint32_t pf_metadata = 0;
    uint32_t cpu = std::numeric_limits<uint32_t>::max();
    uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

    std::size_t translation_level = 0;

    mshr_type(const request_type& req, std::size_t level);
  };

  std::deque<mshr_type> MSHR;
  std::deque<mshr_type> finished;
  std::deque<mshr_type> completed;
  // A response can match several walks. Remaining matches stay at the front
  // of the MSHR queue until all of them enter refill staging.
  std::size_t pending_return_matches = 0;

  std::vector<channel_type*> upper_levels;
  channel_type* lower_level;

  std::optional<mshr_type> handle_read(const request_type& pkt, channel_type* ul);
  std::optional<mshr_type> handle_fill(const mshr_type& fill_mshr);
  std::optional<mshr_type> step_translation(const mshr_type& source);

  bool finish_packet(const response_type& packet);

public:
  struct walk_cache_geometry {
    std::size_t level;
    std::size_t sets;
    std::size_t ways;
    unsigned offset_bits;
  };

  const std::string NAME;
  const uint32_t MSHR_SIZE;
  const std::optional<uint32_t> REFILL_QUEUE_SIZE;
  champsim::bandwidth::maximum_type MAX_READ, MAX_FILL;
  const champsim::chrono::clock::duration HIT_LATENCY;

  std::vector<pscl_type> pscl;
  std::vector<walk_cache_geometry> walk_cache_geometries;
  std::vector<champsim::walk_cache_activity> walk_cache_accesses;
  std::optional<champsim::memory_activity> memory_activity;
  VirtualMemory* vmem;

  void record_memory(champsim::memory_event event, std::size_t count = 1)
  {
    if (memory_activity)
      memory_activity->record(event, count);
  }

  [[nodiscard]] const auto& request_channels() const { return upper_levels; }

  const champsim::address CR3_addr;

  explicit PageTableWalker(champsim::ptw_builder builder);

  long operate() final;

  void begin_phase() final;
  void print_deadlock() final;
};

#endif
