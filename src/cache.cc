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

#include "cache.h"
#include "storage.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <fmt/core.h>

#include "bandwidth.h"
#include "champsim.h"
#include "chrono.h"
#include "deadlock.h"
#include "instruction.h"
#include "util/algorithm.h"
#include "util/bits.h"
#include "util/span.h"

CACHE::CACHE(CACHE&& other)
    : operable(other),

      upper_levels(std::move(other.upper_levels)), lower_level(std::move(other.lower_level)), lower_translate(std::move(other.lower_translate)),

      cpu(other.cpu), NAME(std::move(other.NAME)), NUM_SET(other.NUM_SET), NUM_WAY(other.NUM_WAY), MSHR_SIZE(other.MSHR_SIZE), PQ_SIZE(other.PQ_SIZE),
      HIT_LATENCY(other.HIT_LATENCY), FILL_LATENCY(other.FILL_LATENCY), OFFSET_BITS(other.OFFSET_BITS), ubs(other.ubs),
      UBS_WAY_SIZES(std::move(other.UBS_WAY_SIZES)), UBS_CANDIDATE_WAYS(other.UBS_CANDIDATE_WAYS), block(std::move(other.block)),
      ubs_predictor(std::move(other.ubs_predictor)), ubs_access_counter(other.ubs_access_counter), MAX_TAG(other.MAX_TAG), MAX_FILL(other.MAX_FILL),
      prefetch_as_load(other.prefetch_as_load), match_offset_bits(other.match_offset_bits), virtual_prefetch(other.virtual_prefetch),
      pref_activate_mask(std::move(other.pref_activate_mask)),

      sim_stats(std::move(other.sim_stats)), roi_stats(std::move(other.roi_stats)),

      pref_module_pimpl(std::move(other.pref_module_pimpl)), repl_module_pimpl(std::move(other.repl_module_pimpl))
{
  pref_module_pimpl->bind(this);
  repl_module_pimpl->bind(this);
}

auto CACHE::operator=(CACHE&& other) -> CACHE&
{
  this->clock_period = other.clock_period;
  this->current_time = other.current_time;
  this->warmup = other.warmup;

  this->upper_levels = std::move(other.upper_levels);
  this->lower_level = std::move(other.lower_level);
  this->lower_translate = std::move(other.lower_translate);

  this->cpu = other.cpu;
  this->NAME = std::move(other.NAME);
  this->NUM_SET = other.NUM_SET;
  this->NUM_WAY = other.NUM_WAY;
  ;
  this->MSHR_SIZE = other.MSHR_SIZE;
  ;
  this->PQ_SIZE = other.PQ_SIZE;
  this->HIT_LATENCY = other.HIT_LATENCY;
  this->FILL_LATENCY = other.FILL_LATENCY;
  this->OFFSET_BITS = other.OFFSET_BITS;
  this->ubs = other.ubs;
  this->UBS_WAY_SIZES = std::move(other.UBS_WAY_SIZES);
  this->UBS_CANDIDATE_WAYS = other.UBS_CANDIDATE_WAYS;
  this->block = std::move(other.block);
  this->ubs_predictor = std::move(other.ubs_predictor);
  this->ubs_access_counter = other.ubs_access_counter;
  this->MAX_TAG = other.MAX_TAG;
  this->MAX_FILL = other.MAX_FILL;
  this->prefetch_as_load = other.prefetch_as_load;
  this->match_offset_bits = other.match_offset_bits;
  this->virtual_prefetch = other.virtual_prefetch;
  this->pref_activate_mask = std::move(other.pref_activate_mask);

  this->sim_stats = std::move(other.sim_stats);
  this->roi_stats = std::move(other.roi_stats);

  this->pref_module_pimpl = std::move(other.pref_module_pimpl);
  this->repl_module_pimpl = std::move(other.repl_module_pimpl);

  pref_module_pimpl->bind(this);
  repl_module_pimpl->bind(this);

  return *this;
}

CACHE::tag_lookup_type::tag_lookup_type(const request_type& req, bool local_pref, bool skip)
    : address(req.address), v_address(req.v_address), data(req.data), ip(req.ip), instr_id(req.instr_id), pf_metadata(req.pf_metadata), cpu(req.cpu),
      type(req.type), prefetch_from_this(local_pref), skip_fill(skip), is_translated(req.is_translated), requested_bytes(req.requested_bytes),
      instr_depend_on_me(req.instr_depend_on_me)
{
}

CACHE::fill_type::fill_type(const tag_lookup_type& req, champsim::chrono::clock::time_point _time_enqueued)
    : address(req.address), v_address(req.v_address), ip(req.ip), instr_id(req.instr_id), cpu(req.cpu), type(req.type),
      prefetch_from_this(req.prefetch_from_this), partial_miss(req.partial_miss), requested_bytes(req.requested_bytes),
      ubs_preserved_bytes(req.ubs_preserved_bytes), time_enqueued(_time_enqueued), instr_depend_on_me(req.instr_depend_on_me), to_return(req.to_return)
{
}

CACHE::fill_type CACHE::fill_type::merge(fill_type predecessor, fill_type successor)
{
  std::vector<uint64_t> merged_instr{};
  std::vector<std::deque<response_type>*> merged_return{};

  std::set_union(std::begin(predecessor.instr_depend_on_me), std::end(predecessor.instr_depend_on_me), std::begin(successor.instr_depend_on_me),
                 std::end(successor.instr_depend_on_me), std::back_inserter(merged_instr));
  std::set_union(std::begin(predecessor.to_return), std::end(predecessor.to_return), std::begin(successor.to_return), std::end(successor.to_return),
                 std::back_inserter(merged_return));

  fill_type retval{(successor.type == access_type::PREFETCH) ? predecessor : successor};

  // set the time enqueued to the predecessor unless its a demand into prefetch, in which case we use the successor
  retval.time_enqueued =
      ((successor.type != access_type::PREFETCH && predecessor.type == access_type::PREFETCH)) ? successor.time_enqueued : predecessor.time_enqueued;
  retval.instr_depend_on_me = merged_instr;
  retval.to_return = merged_return;
  retval.data_promise = predecessor.data_promise;
  retval.partial_miss = predecessor.partial_miss || successor.partial_miss;
  retval.requested_bytes = predecessor.requested_bytes | successor.requested_bytes;
  retval.ubs_preserved_bytes = predecessor.ubs_preserved_bytes | successor.ubs_preserved_bytes;

  if constexpr (champsim::debug_print) {
    if (successor.type == access_type::PREFETCH) {
      fmt::print("[MSHR] {} address {} type: {} into address {} type: {}\n", __func__, successor.address,
                 access_type_names.at(champsim::to_underlying(successor.type)), predecessor.address,
                 access_type_names.at(champsim::to_underlying(successor.type)));
    } else {
      fmt::print("[MSHR] {} address {} type: {} into address {} type: {}\n", __func__, predecessor.address,
                 access_type_names.at(champsim::to_underlying(predecessor.type)), successor.address,
                 access_type_names.at(champsim::to_underlying(successor.type)));
    }
  }

  return retval;
}

auto CACHE::fill_block(fill_type fill, uint32_t metadata) -> BLOCK
{
  CACHE::BLOCK to_fill;
  to_fill.valid = true;
  to_fill.prefetch = fill.prefetch_from_this;
  to_fill.dirty = (fill.type == access_type::WRITE);
  to_fill.address = fill.address;
  to_fill.v_address = fill.v_address;
  to_fill.data = fill.data_promise->data;
  to_fill.pf_metadata = metadata;

  return to_fill;
}

auto CACHE::matches_address(champsim::address addr) const
{
  return [match = addr.slice_upper(OFFSET_BITS), shamt = OFFSET_BITS](const auto& entry) {
    return entry.address.slice_upper(shamt) == match;
  };
}

uint64_t CACHE::get_requested_bytes(const tag_lookup_type& packet) const
{
  if (packet.requested_bytes != 0)
    return packet.requested_bytes;

  const auto request_address = packet.v_address != champsim::address{} ? packet.v_address : packet.address;
  const auto offset = request_address.slice_lower(OFFSET_BITS).to<uint32_t>();
  return uint64_t{1} << offset;
}

uint64_t CACHE::get_ubs_block_mask(const BLOCK& cache_block) const
{
  if (cache_block.size >= 64)
    return std::numeric_limits<uint64_t>::max();
  return ((uint64_t{1} << cache_block.size) - 1) << cache_block.offset;
}

uint64_t CACHE::install_ubs_subblock(const ubs_predictor_entry& victim, uint32_t offset, uint32_t requested_size)
{
  auto first_candidate = std::find_if(std::begin(UBS_WAY_SIZES), std::end(UBS_WAY_SIZES), [requested_size](auto size) { return size >= requested_size; });
  if (first_candidate == std::end(UBS_WAY_SIZES))
    return 0;

  const auto first_way = static_cast<std::size_t>(std::distance(std::begin(UBS_WAY_SIZES), first_candidate));
  // Historical BOUND4 counts size transitions, including the first way of
  // the class which reaches the bound, rather than four physical ways.
  auto last_way = first_way;
  uint32_t transitions = 0;
  auto previous_size = UBS_WAY_SIZES.at(first_way);
  while (last_way < NUM_WAY && transitions < UBS_CANDIDATE_WAYS) {
    if (UBS_WAY_SIZES.at(last_way) != previous_size) {
      previous_size = UBS_WAY_SIZES.at(last_way);
      ++transitions;
    }
    ++last_way;
  }
  auto [set_begin, set_end] = get_set_span(victim.block.address);
  auto candidate_begin = std::next(set_begin, static_cast<set_type::difference_type>(first_way));
  auto candidate_end = std::next(set_begin, static_cast<set_type::difference_type>(last_way));

  // Empty fitting slots anywhere in the set take precedence over replacement.
  auto way = std::find_if_not(candidate_begin, set_end, [](const auto& entry) { return entry.valid; });
  if (way == set_end) {
    way = std::max_element(candidate_begin, candidate_end, [](const auto& lhs, const auto& rhs) { return lhs.last_used < rhs.last_used; });
  }

  const auto way_idx = static_cast<std::size_t>(std::distance(set_begin, way));
  const auto capacity = UBS_WAY_SIZES.at(way_idx);
  const auto line_size = uint32_t{1} << champsim::to_underlying(OFFSET_BITS);
  const auto stored_offset = std::min(offset, line_size - capacity);

  champsim::address evicting_address{};
  if (way->valid) {
    evicting_address = module_address(*way);
    if (way->prefetch)
      ++sim_stats.pf_useless;
  }

  const auto metadata = impl_prefetcher_cache_fill(module_address(victim.block), get_set_index(victim.block.address), static_cast<long>(way_idx), false,
                                                   evicting_address, victim.block.pf_metadata);

  // Reproduce LRU2 insertion and its historical physical-way rank-update
  // subset, which starts at the selected slot (not the victim-search window).
  const auto insertion_rank = NUM_WAY > 1 ? NUM_WAY - 2 : 0;
  const auto update_end = std::next(set_begin, static_cast<set_type::difference_type>(std::min<std::size_t>(NUM_WAY, way_idx + UBS_CANDIDATE_WAYS)));
  for (auto entry = way; entry != update_end; ++entry)
    if (entry->last_used > 0 && entry->last_used <= insertion_rank && entry->last_used <= NUM_WAY)
      ++entry->last_used;

  *way = victim.block;
  way->valid = true;
  way->prefetch = false;
  way->dirty = false;
  way->offset = static_cast<uint8_t>(stored_offset);
  way->size = static_cast<uint8_t>(capacity);
  way->last_used = insertion_rank;
  way->pf_metadata = metadata;

  return get_ubs_block_mask(*way);
}

void CACHE::install_ubs_predictor_victim(const ubs_predictor_entry& victim)
{
  if (!victim.block.valid)
    return;

  if (victim.accessed_bytes == 0) {
    if (victim.block.prefetch)
      ++sim_stats.pf_useless;
    return;
  }

  auto remaining = victim.accessed_bytes;
  while (remaining != 0) {
    uint32_t first = 0;
    while (((remaining >> first) & 1) == 0)
      ++first;

    uint32_t count = 0;
    while (first + count < 64 && ((remaining >> (first + count)) & 1) != 0)
      ++count;

    const auto inserted = install_ubs_subblock(victim, first, count);
    if (inserted == 0)
      break;
    remaining &= ~inserted;
  }
}

template <typename T>
champsim::address CACHE::module_address(const T& element) const
{
  auto address = virtual_prefetch ? element.v_address : element.address;
  return champsim::address{address.slice_upper(match_offset_bits ? champsim::data::bits{} : OFFSET_BITS)};
}

bool CACHE::handle_fill(const fill_type& fill)
{
  if (ubs)
    return handle_ubs_fill(fill);

  cpu = fill.cpu;

  // find victim
  auto [set_begin, set_end] = get_set_span(fill.address);
  auto way = std::find_if_not(set_begin, set_end, [](auto x) { return x.valid; });
  if (way == set_end) {
    way = std::next(set_begin, impl_find_victim(fill.cpu, fill.instr_id, get_set_index(fill.address), &*set_begin, fill.ip, fill.address, fill.type));
  }
  assert(set_begin <= way);
  assert(way <= set_end);
  assert(way != set_end || fill.type != access_type::WRITE); // Writes may not bypass
  const auto way_idx = std::distance(set_begin, way);        // cast protected by earlier assertion

  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {} v_address: {} set: {} way: {} type: {} prefetch_metadata: {} cycle_enqueued: {} cycle: {}\n", NAME, __func__,
               fill.instr_id, fill.address, fill.v_address, get_set_index(fill.address), way_idx, access_type_names.at(champsim::to_underlying(fill.type)),
               fill.data_promise->pf_metadata, (fill.time_enqueued.time_since_epoch()) / clock_period, (current_time.time_since_epoch()) / clock_period);
  }

  if (way != set_end && way->valid && way->dirty) {
    request_type writeback_packet;

    writeback_packet.cpu = fill.cpu;
    writeback_packet.address = way->address;
    writeback_packet.data = way->data;
    writeback_packet.instr_id = fill.instr_id;
    writeback_packet.ip = champsim::address{};
    writeback_packet.type = access_type::WRITE;
    writeback_packet.pf_metadata = way->pf_metadata;
    writeback_packet.response_requested = false;

    if constexpr (champsim::debug_print) {
      fmt::print("[{}] {} evict address: {} v_address: {} prefetch_metadata: {}\n", NAME, __func__, writeback_packet.address, writeback_packet.v_address,
                 fill.data_promise->pf_metadata);
    }

    auto success = lower_level->add_wq(writeback_packet);
    if (!success) {
      return false;
    }
  }

  champsim::address evicting_address{};
  if (way != set_end && way->valid) {
    evicting_address = module_address(*way);
  }

  auto metadata_thru = impl_prefetcher_cache_fill(module_address(fill), get_set_index(fill.address), way_idx, (fill.type == access_type::PREFETCH),
                                                  evicting_address, fill.data_promise->pf_metadata);
  impl_replacement_cache_fill(fill.cpu, get_set_index(fill.address), way_idx, module_address(fill), fill.ip, evicting_address, fill.type);

  if (way != set_end) {
    if (way->valid && way->prefetch) {
      ++sim_stats.pf_useless;
    }

    if (fill.type == access_type::PREFETCH) {
      ++sim_stats.pf_fill;
    }

    *way = fill_block(fill, metadata_thru);
  }

  // COLLECT STATS
  if (fill.type != access_type::PREFETCH)
    sim_stats.total_miss_latency_cycles += (current_time - (fill.time_enqueued + clock_period)) / clock_period;
  sim_stats.fill.increment(std::pair{fill.type, fill.cpu});

  response_type response{fill.address, fill.v_address, fill.data_promise->data, metadata_thru, fill.instr_depend_on_me};
  for (auto* ret : fill.to_return) {
    ret->push_back(response);
  }

  return true;
}

bool CACHE::handle_ubs_fill(const fill_type& fill)
{
  cpu = fill.cpu;

  const auto set = get_set_index(fill.address);
  auto& predictor = ubs_predictor.at(static_cast<std::size_t>(set));
  const auto same_predictor_line = predictor.block.valid && matches_address(fill.address)(predictor.block);

  if (!same_predictor_line) {
    // Capture any sub-blocks which appeared after the miss was accepted before
    // installing the old predictor victim. The victim insertion may otherwise
    // overwrite one of these ways before its useful-byte mask is preserved.
    auto preserved_bytes = fill.ubs_preserved_bytes;
    auto [set_begin, set_end] = get_set_span(fill.address);
    for (auto way = set_begin; way != set_end; ++way) {
      if (way->valid && matches_address(fill.address)(*way)) {
        preserved_bytes |= get_ubs_block_mask(*way);
        way->valid = false;
      }
    }

    install_ubs_predictor_victim(predictor);
    predictor = {};
    predictor.accessed_bytes = preserved_bytes;
    predictor.block = fill_block(fill, fill.data_promise->pf_metadata);
    predictor.block.offset = 0;
    predictor.block.size = static_cast<uint8_t>(uint32_t{1} << champsim::to_underlying(OFFSET_BITS));
    predictor.ip = fill.ip;
    predictor.instr_id = fill.instr_id;
    predictor.cpu = fill.cpu;
    predictor.type = fill.type;
  }
  predictor.accessed_bytes |= fill.ubs_preserved_bytes;

  if (fill.type != access_type::PREFETCH) {
    uint64_t requested = fill.requested_bytes;
    if (requested == 0) {
      const auto request_address = fill.v_address != champsim::address{} ? fill.v_address : fill.address;
      requested = uint64_t{1} << request_address.slice_lower(OFFSET_BITS).to<uint32_t>();
    }
    predictor.accessed_bytes |= requested;

    if (predictor.block.prefetch) {
      ++sim_stats.pf_useful;
      predictor.block.prefetch = false;
    }
  }

  const auto metadata = impl_prefetcher_cache_fill(module_address(fill), set, NUM_WAY, fill.type == access_type::PREFETCH, {}, fill.data_promise->pf_metadata);
  predictor.block.pf_metadata = metadata;
  predictor.block.data = fill.data_promise->data;

  if (fill.type == access_type::PREFETCH)
    ++sim_stats.pf_fill;

  if (fill.type != access_type::PREFETCH)
    sim_stats.total_miss_latency_cycles += (current_time - (fill.time_enqueued + clock_period)) / clock_period;
  sim_stats.fill.increment(std::pair{fill.type, fill.cpu});

  response_type response{fill.address, fill.v_address, fill.data_promise->data, metadata, fill.instr_depend_on_me};
  for (auto* ret : fill.to_return)
    ret->push_back(response);

  return true;
}

bool CACHE::try_hit(tag_lookup_type& handle_pkt)
{
  cpu = handle_pkt.cpu;

  if (ubs) {
    const auto requested = get_requested_bytes(handle_pkt);
    handle_pkt.requested_bytes = requested;
    handle_pkt.partial_miss = false;
    handle_pkt.ubs_preserved_bytes = 0;
    auto& predictor = ubs_predictor.at(static_cast<std::size_t>(get_set_index(handle_pkt.address)));

    bool hit = predictor.block.valid && matches_address(handle_pkt.address)(predictor.block);
    bool useful_prefetch = hit && predictor.block.prefetch && !handle_pkt.prefetch_from_this;
    champsim::address response_data = predictor.block.data;

    auto [set_begin, set_end] = get_set_span(handle_pkt.address);
    if (!hit) {
      uint64_t present_bytes = 0;
      bool matching_tag = false;
      for (auto way = set_begin; way != set_end; ++way) {
        if (way->valid && matches_address(handle_pkt.address)(*way)) {
          matching_tag = true;
          present_bytes |= get_ubs_block_mask(*way);
          response_data = way->data;
          useful_prefetch |= way->prefetch && (get_ubs_block_mask(*way) & requested) != 0 && !handle_pkt.prefetch_from_this;
        }
      }

      hit = (present_bytes & requested) == requested;
      useful_prefetch &= hit;
      handle_pkt.partial_miss = matching_tag && !hit;
      if (handle_pkt.partial_miss)
        handle_pkt.ubs_preserved_bytes = present_bytes;

      if (hit) {
        for (auto way = set_begin; way != set_end; ++way) {
          if (way->valid && matches_address(handle_pkt.address)(*way) && (get_ubs_block_mask(*way) & requested) != 0) {
            const auto hit_rank = way->last_used;
            for (auto entry = set_begin; entry != set_end; ++entry)
              if (entry->last_used < NUM_WAY && entry->last_used <= hit_rank)
                ++entry->last_used;
            way->last_used = 0;
            if (way->prefetch && !handle_pkt.prefetch_from_this)
              way->prefetch = false;
          }
        }
      }
    } else if (!handle_pkt.prefetch_from_this) {
      predictor.accessed_bytes |= requested;
      predictor.block.last_used = ++ubs_access_counter;
      if (predictor.block.prefetch)
        predictor.block.prefetch = false;
    }

    auto metadata_thru = handle_pkt.pf_metadata;
    if (should_activate_prefetcher(handle_pkt)) {
      // A tag-only match is deliberately reported as a miss. This preserves
      // the current prefetcher/predictor training behavior for partial misses.
      metadata_thru = impl_prefetcher_cache_operate(module_address(handle_pkt), handle_pkt.ip, hit, useful_prefetch, handle_pkt.type, metadata_thru);
    }

    if (hit) {
      sim_stats.hits.increment(std::pair{handle_pkt.type, handle_pkt.cpu});
      if (useful_prefetch)
        ++sim_stats.pf_useful;

      response_type response{handle_pkt.address, handle_pkt.v_address, response_data, metadata_thru, handle_pkt.instr_depend_on_me};
      for (auto* ret : handle_pkt.to_return)
        ret->push_back(response);
    }

    return hit;
  }

  // access cache
  auto [set_begin, set_end] = get_set_span(handle_pkt.address);
  auto way = std::find_if(set_begin, set_end, [matcher = matches_address(handle_pkt.address)](const auto& x) { return x.valid && matcher(x); });
  const auto hit = (way != set_end);
  const auto useful_prefetch = (hit && way->prefetch && !handle_pkt.prefetch_from_this);

  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {} v_address: {} data: {} set: {} way: {} ({}) type: {} cycle: {}\n", NAME, __func__, handle_pkt.instr_id,
               handle_pkt.address, handle_pkt.v_address, handle_pkt.data, get_set_index(handle_pkt.address), std::distance(set_begin, way),
               hit ? "HIT" : "MISS", access_type_names.at(champsim::to_underlying(handle_pkt.type)), current_time.time_since_epoch() / clock_period);
  }

  auto metadata_thru = handle_pkt.pf_metadata;
  if (should_activate_prefetcher(handle_pkt)) {
    metadata_thru = impl_prefetcher_cache_operate(module_address(handle_pkt), handle_pkt.ip, hit, useful_prefetch, handle_pkt.type, metadata_thru);
  }

  // update replacement policy
  const auto way_idx = std::distance(set_begin, way);
  impl_update_replacement_state(handle_pkt.cpu, get_set_index(handle_pkt.address), way_idx, module_address(handle_pkt), handle_pkt.ip, {}, handle_pkt.type,
                                hit);

  if (hit) {
    sim_stats.hits.increment(std::pair{handle_pkt.type, handle_pkt.cpu});

    response_type response{handle_pkt.address, handle_pkt.v_address, way->data, metadata_thru, handle_pkt.instr_depend_on_me};
    for (auto* ret : handle_pkt.to_return) {
      ret->push_back(response);
    }

    way->dirty |= (handle_pkt.type == access_type::WRITE);

    // update prefetch stats and reset prefetch bit
    if (useful_prefetch) {
      ++sim_stats.pf_useful;
      way->prefetch = false;
    }
  }

  return hit;
}

auto CACHE::mshr_and_forward_packet(const tag_lookup_type& handle_pkt) -> std::pair<fill_type, request_type>
{
  fill_type to_allocate{handle_pkt, current_time};

  request_type fwd_pkt;

  fwd_pkt.asid[0] = handle_pkt.asid[0];
  fwd_pkt.asid[1] = handle_pkt.asid[1];
  fwd_pkt.type = (handle_pkt.type == access_type::WRITE) ? access_type::RFO : handle_pkt.type;
  fwd_pkt.pf_metadata = handle_pkt.pf_metadata;
  fwd_pkt.cpu = handle_pkt.cpu;

  fwd_pkt.address = handle_pkt.address;
  fwd_pkt.v_address = handle_pkt.v_address;
  fwd_pkt.data = handle_pkt.data;
  fwd_pkt.instr_id = handle_pkt.instr_id;
  fwd_pkt.ip = handle_pkt.ip;
  fwd_pkt.requested_bytes = handle_pkt.requested_bytes;

  fwd_pkt.instr_depend_on_me = handle_pkt.instr_depend_on_me;
  fwd_pkt.response_requested = (!handle_pkt.prefetch_from_this || !handle_pkt.skip_fill);

  return std::pair{std::move(to_allocate), std::move(fwd_pkt)};
}

bool CACHE::handle_miss(const tag_lookup_type& handle_pkt)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {} v_address: {} type: {} local_prefetch: {} cycle: {}\n", NAME, __func__, handle_pkt.instr_id,
               handle_pkt.address, handle_pkt.v_address, access_type_names.at(champsim::to_underlying(handle_pkt.type)), handle_pkt.prefetch_from_this,
               current_time.time_since_epoch() / clock_period);
  }

  fill_type to_allocate{handle_pkt, current_time};

  cpu = handle_pkt.cpu;

  auto mshr_pkt = mshr_and_forward_packet(handle_pkt);

  // check mshr
  auto fill_entry = std::find_if(std::begin(MSHR), std::end(MSHR), matches_address(handle_pkt.address));
  bool mshr_full = (MSHR.size() == MSHR_SIZE);
  bool fill_already_pending = fill_entry != MSHR.end();

  // check inflight fills
  if (!fill_already_pending) {
    fill_entry = std::find_if(inflight_fills.begin(), inflight_fills.end(), matches_address(handle_pkt.address));
    fill_already_pending = fill_entry != inflight_fills.end();
  }

  if (fill_already_pending) // miss or fill already inflight
  {
    if (fill_entry->type == access_type::PREFETCH && handle_pkt.type != access_type::PREFETCH) {
      // Mark the prefetch as useful
      if (fill_entry->prefetch_from_this) {
        ++sim_stats.pf_useful;
      }
    }

    // COLLECT STATS
    sim_stats.miss_merge.increment(std::pair{to_allocate.type, to_allocate.cpu});

    *fill_entry = fill_type::merge(*fill_entry, to_allocate);
  } else {
    if (mshr_full) { // not enough MSHR resource
      return false;  // TODO should we allow prefetches anyway if they will not be filled to this level?
    }

    const bool send_to_rq = (prefetch_as_load || handle_pkt.type != access_type::PREFETCH);
    bool success = send_to_rq ? lower_level->add_rq(mshr_pkt.second) : lower_level->add_pq(mshr_pkt.second);

    if (!success) {
      return false;
    }

    // Allocate an MSHR
    if (mshr_pkt.second.response_requested) {
      MSHR.emplace_back(std::move(mshr_pkt.first));
    }
  }

  if (ubs && handle_pkt.partial_miss && (fill_already_pending || mshr_pkt.second.response_requested)) {
    // Once the miss is guaranteed to make progress, remove the old fragments.
    // Their coverage travels with the MSHR and is restored in the predictor on
    // fill, preventing both duplicates and loss from intervening evictions.
    auto [set_begin, set_end] = get_set_span(handle_pkt.address);
    for (auto way = set_begin; way != set_end; ++way) {
      if (way->valid && matches_address(handle_pkt.address)(*way))
        way->valid = false;
    }
  }

  sim_stats.misses.increment(std::pair{handle_pkt.type, handle_pkt.cpu});
  if (handle_pkt.partial_miss)
    sim_stats.partial_misses.increment(std::pair{handle_pkt.type, handle_pkt.cpu});

  return true;
}

bool CACHE::handle_write(const tag_lookup_type& handle_pkt)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {} v_address: {} type: {} local_prefetch: {} cycle: {}\n", NAME, __func__, handle_pkt.instr_id,
               handle_pkt.address, handle_pkt.v_address, access_type_names.at(champsim::to_underlying(handle_pkt.type)), handle_pkt.prefetch_from_this,
               current_time.time_since_epoch() / clock_period);
  }

  fill_type to_allocate{handle_pkt, current_time};
  to_allocate.data_promise.ready_at(current_time + (warmup ? champsim::chrono::clock::duration{} : FILL_LATENCY));
  inflight_fills.push_back(to_allocate);

  sim_stats.misses.increment(std::pair{handle_pkt.type, handle_pkt.cpu});

  return true;
}

template <bool UpdateRequest>
auto CACHE::initiate_tag_check(champsim::channel* ul)
{
  return [time = current_time + (warmup ? champsim::chrono::clock::duration{} : HIT_LATENCY), ul](const auto& entry) {
    CACHE::tag_lookup_type retval{entry};
    retval.event_cycle = time;

    if constexpr (UpdateRequest) {
      if (entry.response_requested) {
        retval.to_return = {&ul->returned};
      }
    } else {
      (void)ul; // supress warning about ul being unused
    }

    if constexpr (champsim::debug_print) {
      fmt::print("[TAG] initiate_tag_check instr_id: {} address: {} v_address: {} type: {} response_requested: {}\n", retval.instr_id, retval.address,
                 retval.v_address, access_type_names.at(champsim::to_underlying(retval.type)), !std::empty(retval.to_return));
    }

    return retval;
  };
}

long CACHE::operate()
{
  long progress{0};

  auto is_ready = [time = current_time](const auto& entry) {
    return entry.event_cycle <= time;
  };
  auto is_translated = [](const auto& entry) {
    return entry.is_translated;
  };

  // Finish returns
  std::for_each(std::cbegin(lower_level->returned), std::cend(lower_level->returned), [this](const auto& pkt) { this->finish_packet(pkt); });
  progress += std::distance(std::cbegin(lower_level->returned), std::cend(lower_level->returned));
  lower_level->returned.clear();

  // Finish translations
  if (lower_translate != nullptr) {
    std::for_each(std::cbegin(lower_translate->returned), std::cend(lower_translate->returned), [this](const auto& pkt) { this->finish_translation(pkt); });
    progress += std::distance(std::cbegin(lower_translate->returned), std::cend(lower_translate->returned));
    lower_translate->returned.clear();
  }

  // Perform fills
  champsim::bandwidth fill_bw{MAX_FILL};
  auto [fill_begin, fill_end] = champsim::get_span_p(std::cbegin(inflight_fills), std::cend(inflight_fills), fill_bw,
                                                     [time = current_time](const auto& x) { return x.data_promise.is_ready_at(time); });
  auto complete_end = std::find_if_not(fill_begin, fill_end, [this](const auto& x) { return this->handle_fill(x); });
  fill_bw.consume(std::distance(fill_begin, complete_end));
  inflight_fills.erase(fill_begin, complete_end);

  // Initiate tag checks
  const champsim::bandwidth::maximum_type bandwidth_from_tag_checks{champsim::to_underlying(MAX_TAG) * (long)(HIT_LATENCY / clock_period)
                                                                    - (long)std::size(inflight_tag_check)};
  champsim::bandwidth initiate_tag_bw{std::clamp(bandwidth_from_tag_checks, champsim::bandwidth::maximum_type{0}, MAX_TAG)};
  auto can_translate = [avail = (std::size(translation_stash) < static_cast<std::size_t>(MSHR_SIZE))](const auto& entry) {
    return avail || entry.is_translated;
  };
  auto stash_bandwidth_consumed =
      champsim::transform_while_n(translation_stash, std::back_inserter(inflight_tag_check), initiate_tag_bw, is_translated, initiate_tag_check<false>());
  initiate_tag_bw.consume(stash_bandwidth_consumed);
  std::vector<long long> channels_bandwidth_consumed{};

  if (std::size(upper_levels) > 1) {
    std::rotate(upper_levels.begin(), upper_levels.begin() + 1, upper_levels.end());
  }

  // upper levels get an equal portion of the remaining bandwidth
  champsim::bandwidth::maximum_type per_upper_bandwidth =
      std::size(upper_levels) >= 1
          ? (champsim::bandwidth::maximum_type)std::max((size_t)initiate_tag_bw.amount_remaining() / std::size(upper_levels), size_t{1})
          : champsim::bandwidth::maximum_type{};

  for (auto* ul : upper_levels) {
    for (auto q : {std::ref(ul->WQ), std::ref(ul->RQ), std::ref(ul->PQ)}) {
      // this needs to be in this loop, we need to ensure that for cases where bandwidth doesn't divide nicely across upstreams,
      // we don't accidentally consume more bandwidth than expected
      champsim::bandwidth per_upper_tag_bw{std::min(per_upper_bandwidth, champsim::bandwidth::maximum_type{initiate_tag_bw.amount_remaining()})};
      auto bandwidth_consumed =
          champsim::transform_while_n(q.get(), std::back_inserter(inflight_tag_check), per_upper_tag_bw, can_translate, initiate_tag_check<true>(ul));
      channels_bandwidth_consumed.push_back(bandwidth_consumed);
      initiate_tag_bw.consume(bandwidth_consumed);
    }
  }

  auto pq_bandwidth_consumed =
      champsim::transform_while_n(internal_PQ, std::back_inserter(inflight_tag_check), initiate_tag_bw, can_translate, initiate_tag_check<false>());
  initiate_tag_bw.consume(pq_bandwidth_consumed);

  // Issue translations
  std::for_each(std::begin(inflight_tag_check), std::end(inflight_tag_check), [this](auto& x) { this->issue_translation(x); });
  std::for_each(std::begin(translation_stash), std::end(translation_stash), [this](auto& x) { this->issue_translation(x); });

  // Find entries that would be ready except that they have not finished translation, move them to the stash
  auto [last_not_missed, stash_end] = champsim::extract_if(std::begin(inflight_tag_check), std::end(inflight_tag_check), std::back_inserter(translation_stash),
                                                           [is_ready, is_translated](const auto& x) { return is_ready(x) && !is_translated(x); });
  progress += std::distance(last_not_missed, std::end(inflight_tag_check));
  inflight_tag_check.erase(last_not_missed, std::end(inflight_tag_check));

  // Perform tag checks
  auto do_handle_miss = [this](const auto& pkt) {
    if (pkt.type == access_type::WRITE && !this->match_offset_bits) {
      return this->handle_write(pkt); // Treat writes (that is, writebacks) like fills
    }
    return this->handle_miss(pkt); // Treat writes (that is, stores) like reads
  };
  champsim::bandwidth tag_check_bw{MAX_TAG};
  auto [tag_check_ready_begin, tag_check_ready_end] =
      champsim::get_span_p(std::begin(inflight_tag_check), std::end(inflight_tag_check), tag_check_bw,
                           [is_ready, is_translated](const auto& pkt) { return is_ready(pkt) && is_translated(pkt); });
  std::for_each(tag_check_ready_begin, tag_check_ready_end, [this](auto& pkt) { pkt.hit = this->try_hit(pkt); });
  auto hits_end = std::stable_partition(tag_check_ready_begin, tag_check_ready_end, [](const auto& pkt) { return pkt.hit; });
  auto finish_tag_check_end = std::stable_partition(hits_end, tag_check_ready_end, do_handle_miss);
  tag_check_bw.consume(std::distance(tag_check_ready_begin, finish_tag_check_end));
  inflight_tag_check.erase(tag_check_ready_begin, finish_tag_check_end);

  impl_prefetcher_cycle_operate();

  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} cycle completed: {} tags checked: {} remaining: {} stash consumed: {} remaining: {} channel consumed: {} pq consumed {} unused consume "
               "bw {}\n",
               NAME, __func__, current_time.time_since_epoch() / clock_period, tag_check_bw.amount_consumed(), std::size(inflight_tag_check),
               stash_bandwidth_consumed, std::size(translation_stash), channels_bandwidth_consumed, pq_bandwidth_consumed, initiate_tag_bw.amount_remaining());
  }

  return progress + fill_bw.amount_consumed() + initiate_tag_bw.amount_consumed() + tag_check_bw.amount_consumed();
}

// LCOV_EXCL_START exclude deprecated function
uint64_t CACHE::get_set(uint64_t address) const { return static_cast<uint64_t>(get_set_index(champsim::address{address})); }
// LCOV_EXCL_STOP

long CACHE::get_set_index(champsim::address address) const { return address.slice(champsim::dynamic_extent{OFFSET_BITS, champsim::lg2(NUM_SET)}).to<long>(); }

template <typename It>
std::pair<It, It> get_span(It anchor, typename std::iterator_traits<It>::difference_type set_idx, typename std::iterator_traits<It>::difference_type num_way)
{
  auto begin = std::next(anchor, set_idx * num_way);
  return {std::move(begin), std::next(begin, num_way)};
}

auto CACHE::get_set_span(champsim::address address) -> std::pair<set_type::iterator, set_type::iterator>
{
  const auto set_idx = get_set_index(address);
  assert(set_idx < NUM_SET);
  return get_span(std::begin(block), static_cast<set_type::difference_type>(set_idx), NUM_WAY); // safe cast because of prior assert
}

auto CACHE::get_set_span(champsim::address address) const -> std::pair<set_type::const_iterator, set_type::const_iterator>
{
  const auto set_idx = get_set_index(address);
  assert(set_idx < NUM_SET);
  return get_span(std::cbegin(block), static_cast<set_type::difference_type>(set_idx), NUM_WAY); // safe cast because of prior assert
}

// LCOV_EXCL_START exclude deprecated function
uint64_t CACHE::get_way(uint64_t address, uint64_t /*unused set index*/) const
{
  champsim::address intern_addr{address};
  auto [begin, end] = get_set_span(intern_addr);
  return static_cast<uint64_t>(std::distance(begin, std::find_if(begin, end, matches_address(champsim::address{address}))));
}
// LCOV_EXCL_STOP

long CACHE::invalidate_entry(champsim::address inval_addr)
{
  auto [begin, end] = get_set_span(inval_addr);
  auto inv_way = std::find_if(begin, end, [matcher = matches_address(inval_addr)](const auto& entry) { return entry.valid && matcher(entry); });

  if (ubs) {
    for (auto way = begin; way != end; ++way) {
      if (way->valid && matches_address(inval_addr)(*way))
        way->valid = false;
    }
    auto& predictor = ubs_predictor.at(static_cast<std::size_t>(get_set_index(inval_addr)));
    if (predictor.block.valid && matches_address(inval_addr)(predictor.block))
      predictor = {};
  } else if (inv_way != end) {
    inv_way->valid = false;
  }

  return std::distance(begin, inv_way);
}

bool CACHE::prefetch_line(champsim::address pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  ++sim_stats.pf_requested;

  if (std::size(internal_PQ) >= PQ_SIZE) {
    return false;
  }

  request_type pf_packet;
  pf_packet.type = access_type::PREFETCH;
  pf_packet.pf_metadata = prefetch_metadata;
  pf_packet.cpu = cpu;
  pf_packet.address = pf_addr;
  pf_packet.v_address = virtual_prefetch ? pf_addr : champsim::address{};
  pf_packet.is_translated = !virtual_prefetch;
  if (ubs)
    pf_packet.requested_bytes = uint64_t{1} << pf_addr.slice_lower(OFFSET_BITS).to<uint32_t>();

  internal_PQ.emplace_back(pf_packet, true, !fill_this_level);
  ++sim_stats.pf_issued;

  return true;
}

// LCOV_EXCL_START exclude deprecated function
bool CACHE::prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  return prefetch_line(champsim::address{pf_addr}, fill_this_level, prefetch_metadata);
}

bool CACHE::prefetch_line(uint64_t /*deprecated*/, uint64_t /*deprecated*/, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  return prefetch_line(champsim::address{pf_addr}, fill_this_level, prefetch_metadata);
}
// LCOV_EXCL_STOP

void CACHE::finish_packet(const response_type& packet)
{
  // check MSHR information
  auto mshr_entry = std::find_if(std::begin(MSHR), std::end(MSHR), matches_address(packet.address));

  // sanity check
  if (mshr_entry == MSHR.end()) {
    fmt::print(stderr, "[{}_MSHR] {} cannot find a matching entry! address: {} v_address: {}\n", NAME, __func__, packet.address, packet.v_address);
    assert(0);
  }

  // MSHR holds the most updated information about this request
  fill_type::returned_value finished_value{packet.data, packet.pf_metadata};
  mshr_entry->data_promise = champsim::waitable{finished_value, current_time + (warmup ? champsim::chrono::clock::duration{} : FILL_LATENCY)};
  if constexpr (champsim::debug_print) {
    fmt::print("[{}_MSHR] finish_packet instr_id: {} address: {} data: {} type: {} current: {}\n", this->NAME, mshr_entry->instr_id, mshr_entry->address,
               mshr_entry->data_promise->data, access_type_names.at(champsim::to_underlying(mshr_entry->type)), current_time.time_since_epoch() / clock_period);
  }

  std::iter_swap(mshr_entry, std::begin(MSHR));
  inflight_fills.push_back(MSHR.front());
  MSHR.pop_front();
}

void CACHE::finish_translation(const response_type& packet)
{
  auto matches_vpage = [page_num = champsim::page_number{packet.v_address}](const auto& entry) {
    return (champsim::page_number{entry.v_address} == page_num) && !entry.is_translated;
  };
  auto mark_translated = [p_page = champsim::page_number{packet.data}, this](auto& entry) {
    [[maybe_unused]] auto old_address = entry.address;
    entry.address = champsim::address{champsim::splice(p_page, champsim::page_offset{entry.v_address})}; // translated address
    entry.is_translated = true;                                                                          // This entry is now translated

    if constexpr (champsim::debug_print) {
      fmt::print("[{}_TRANSLATE] finish_translation old: {} paddr: {} vaddr: {} type: {} cycle: {}\n", this->NAME, old_address, entry.address, entry.v_address,
                 access_type_names.at(champsim::to_underlying(entry.type)), this->current_time.time_since_epoch() / this->clock_period);
    }
  };

  // Restart stashed translations
  auto finish_begin = std::find_if_not(std::begin(translation_stash), std::end(translation_stash), [](const auto& x) { return x.is_translated; });
  auto finish_end = std::stable_partition(finish_begin, std::end(translation_stash), matches_vpage);
  std::for_each(finish_begin, finish_end, mark_translated);

  // Find all packets that match the page of the returned packet
  for (auto& entry : inflight_tag_check) {
    if (matches_vpage(entry)) {
      mark_translated(entry);
    }
  }
}

void CACHE::issue_translation(tag_lookup_type& q_entry) const
{
  if (!q_entry.translate_issued && !q_entry.is_translated) {
    request_type fwd_pkt;
    fwd_pkt.asid[0] = q_entry.asid[0];
    fwd_pkt.asid[1] = q_entry.asid[1];
    fwd_pkt.type = access_type::LOAD;
    fwd_pkt.cpu = q_entry.cpu;

    fwd_pkt.address = q_entry.address;
    fwd_pkt.v_address = q_entry.v_address;
    fwd_pkt.data = q_entry.data;
    fwd_pkt.instr_id = q_entry.instr_id;
    fwd_pkt.ip = q_entry.ip;

    fwd_pkt.instr_depend_on_me = q_entry.instr_depend_on_me;
    fwd_pkt.is_translated = true;

    q_entry.translate_issued = lower_translate->add_rq(fwd_pkt);
    if constexpr (champsim::debug_print) {
      if (q_entry.translate_issued) {
        fmt::print("[TRANSLATE] do_issue_translation instr_id: {} paddr: {} vaddr: {} type: {}\n", q_entry.instr_id, q_entry.address, q_entry.v_address,
                   access_type_names.at(champsim::to_underlying(q_entry.type)));
      }
    }
  }
}

std::size_t CACHE::get_mshr_occupancy() const { return std::size(MSHR); }

std::vector<std::size_t> CACHE::get_rq_occupancy() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->rq_occupancy(); });
  return retval;
}

std::vector<std::size_t> CACHE::get_wq_occupancy() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->wq_occupancy(); });
  return retval;
}

std::vector<std::size_t> CACHE::get_pq_occupancy() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->pq_occupancy(); });
  retval.push_back(std::size(internal_PQ));
  return retval;
}

// LCOV_EXCL_START exclude deprecated function
std::size_t CACHE::get_occupancy(uint8_t queue_type, uint64_t /*deprecated*/) const
{
  if (queue_type == 0) {
    return get_mshr_occupancy();
  }
  return 0;
}

std::size_t CACHE::get_occupancy(uint8_t queue_type, champsim::address /*deprecated*/) const
{
  if (queue_type == 0) {
    return get_mshr_occupancy();
  }
  return 0;
}
// LCOV_EXCL_STOP

std::size_t CACHE::get_mshr_size() const { return MSHR_SIZE; }
std::vector<std::size_t> CACHE::get_rq_size() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->rq_size(); });
  return retval;
}

std::vector<std::size_t> CACHE::get_wq_size() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->wq_size(); });
  return retval;
}

std::vector<std::size_t> CACHE::get_pq_size() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->pq_size(); });
  retval.push_back(PQ_SIZE);
  return retval;
}

// LCOV_EXCL_START exclude deprecated function
std::size_t CACHE::get_size(uint8_t queue_type, champsim::address /*deprecated*/) const
{
  if (queue_type == 0) {
    return get_mshr_size();
  }
  return 0;
}

std::size_t CACHE::get_size(uint8_t queue_type, uint64_t /*deprecated*/) const
{
  if (queue_type == 0) {
    return get_mshr_size();
  }
  return 0;
}
// LCOV_EXCL_STOP

namespace
{
double occupancy_ratio(std::size_t occ, std::size_t sz) { return std::ceil(occ) / std::ceil(sz); }

std::vector<double> occupancy_ratio_vec(std::vector<std::size_t> occ, std::vector<std::size_t> sz)
{
  std::vector<double> retval;
  std::transform(std::begin(occ), std::end(occ), std::begin(sz), std::back_inserter(retval), occupancy_ratio);
  return retval;
}
} // namespace

double CACHE::get_mshr_occupancy_ratio() const { return ::occupancy_ratio(get_mshr_occupancy(), get_mshr_size()); }

std::vector<double> CACHE::get_rq_occupancy_ratio() const { return ::occupancy_ratio_vec(get_rq_occupancy(), get_rq_size()); }

std::vector<double> CACHE::get_wq_occupancy_ratio() const { return ::occupancy_ratio_vec(get_wq_occupancy(), get_wq_size()); }

std::vector<double> CACHE::get_pq_occupancy_ratio() const { return ::occupancy_ratio_vec(get_pq_occupancy(), get_pq_size()); }

void CACHE::impl_prefetcher_initialize() const { pref_module_pimpl->impl_prefetcher_initialize(); }

uint32_t CACHE::impl_prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch, access_type type,
                                              uint32_t metadata_in) const
{
  return pref_module_pimpl->impl_prefetcher_cache_operate(addr, ip, cache_hit, useful_prefetch, type, metadata_in);
}

uint32_t CACHE::impl_prefetcher_cache_fill(champsim::address addr, long set, long way, bool prefetch, champsim::address evicted_addr,
                                           uint32_t metadata_in) const
{
  return pref_module_pimpl->impl_prefetcher_cache_fill(addr, set, way, prefetch, evicted_addr, metadata_in);
}

void CACHE::impl_prefetcher_cycle_operate() const { pref_module_pimpl->impl_prefetcher_cycle_operate(); }

void CACHE::impl_prefetcher_final_stats() const { pref_module_pimpl->impl_prefetcher_final_stats(); }

void CACHE::impl_prefetcher_branch_operate(champsim::address ip, uint8_t branch_type, champsim::address branch_target) const
{
  pref_module_pimpl->impl_prefetcher_branch_operate(ip, branch_type, branch_target);
}

void CACHE::impl_initialize_replacement() const { repl_module_pimpl->impl_initialize_replacement(); }

long CACHE::impl_find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const BLOCK* current_set, champsim::address ip, champsim::address full_addr,
                             access_type type) const
{
  return repl_module_pimpl->impl_find_victim(triggering_cpu, instr_id, set, current_set, ip, full_addr, type);
}

void CACHE::impl_update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                          champsim::address victim_addr, access_type type, bool hit) const
{
  repl_module_pimpl->impl_update_replacement_state(triggering_cpu, set, way, full_addr, ip, victim_addr, type, hit);
}

void CACHE::impl_replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                        champsim::address victim_addr, access_type type) const
{
  repl_module_pimpl->impl_replacement_cache_fill(triggering_cpu, set, way, full_addr, ip, victim_addr, type);
}

void CACHE::impl_replacement_final_stats() const { repl_module_pimpl->impl_replacement_final_stats(); }

void CACHE::initialize()
{
  // Report every cache, including L1I even when it has a custom name.
  const auto offset_bits = champsim::to_underlying(OFFSET_BITS);
  const auto tag_bits = 64 - offset_bits - champsim::storage_bits(NUM_SET);
  const uint64_t line_bytes = uint64_t{1} << offset_bits;
  const uint64_t bytes_per_set = ubs ? std::accumulate(UBS_WAY_SIZES.begin(), UBS_WAY_SIZES.end(), uint64_t{0}) : NUM_WAY * line_bytes;
  const uint64_t data = NUM_SET * bytes_per_set * 8;
  const uint64_t count = uint64_t{NUM_SET} * NUM_WAY;
  const uint64_t tags = count * tag_bits;
  const uint64_t status = count * 3; // valid, dirty, prefetch
  // UBS replaces subblocks with its own LRU ranks; other caches use
  // the configured replacement module's model, rather than assuming LRU.
  const auto policy_bits = repl_module_pimpl->impl_replacement_storage_bits();
  const uint64_t replacement = ubs ? count * champsim::storage_bits(NUM_WAY) : policy_bits.value_or(0);
  const auto report_name = NAME + ((!ubs && !policy_bits) ? " [partial: replacement storage unavailable]" : "");
  if (ubs) {
    champsim::print_storage(report_name + " UBS", {
        {"Cache data", data}, {"Cache tags", tags}, {"Cache valid/dirty/prefetch", status},
        {"Cache replacement policy state", replacement}, {"Subblock offsets", count * offset_bits},
        {"Predictor data (one full line per set)", uint64_t{NUM_SET} * line_bytes * 8},
        {"Predictor tags", uint64_t{NUM_SET} * tag_bits}, {"Predictor valid/dirty/prefetch", uint64_t{NUM_SET} * 3},
        {"Predictor useful-byte masks", uint64_t{NUM_SET} * line_bytes}});
  } else {
    champsim::print_storage(report_name + " conventional", {
        {"Cache data", data}, {"Cache tags", tags}, {"Cache valid/dirty/prefetch", status},
        {"Cache replacement policy state", replacement}});
  }
  impl_prefetcher_initialize();
  impl_initialize_replacement();
}

void CACHE::begin_phase()
{
  stats_type new_roi_stats;
  stats_type new_sim_stats;

  new_roi_stats.name = NAME;
  new_sim_stats.name = NAME;

  roi_stats = new_roi_stats;
  sim_stats = new_sim_stats;

  for (auto* ul : upper_levels) {
    channel_type::stats_type ul_new_roi_stats;
    channel_type::stats_type ul_new_sim_stats;
    ul->roi_stats = ul_new_roi_stats;
    ul->sim_stats = ul_new_sim_stats;
  }
}

void CACHE::end_phase(unsigned finished_cpu)
{
  finished_cpu = finished_cpu;
  roi_stats.total_miss_latency_cycles = sim_stats.total_miss_latency_cycles;

  roi_stats.hits = sim_stats.hits;
  roi_stats.misses = sim_stats.misses;
  roi_stats.partial_misses = sim_stats.partial_misses;
  roi_stats.miss_merge = sim_stats.miss_merge;
  roi_stats.fill = sim_stats.fill;

  roi_stats.pf_requested = sim_stats.pf_requested;
  roi_stats.pf_issued = sim_stats.pf_issued;
  roi_stats.pf_useful = sim_stats.pf_useful;
  roi_stats.pf_useless = sim_stats.pf_useless;
  roi_stats.pf_fill = sim_stats.pf_fill;

  for (auto* ul : upper_levels) {
    ul->roi_stats.RQ_ACCESS = ul->sim_stats.RQ_ACCESS;
    ul->roi_stats.RQ_FULL = ul->sim_stats.RQ_FULL;
    ul->roi_stats.RQ_TO_CACHE = ul->sim_stats.RQ_TO_CACHE;

    ul->roi_stats.PQ_ACCESS = ul->sim_stats.PQ_ACCESS;
    ul->roi_stats.PQ_FULL = ul->sim_stats.PQ_FULL;
    ul->roi_stats.PQ_TO_CACHE = ul->sim_stats.PQ_TO_CACHE;

    ul->roi_stats.WQ_ACCESS = ul->sim_stats.WQ_ACCESS;
    ul->roi_stats.WQ_FULL = ul->sim_stats.WQ_FULL;
    ul->roi_stats.WQ_TO_CACHE = ul->sim_stats.WQ_TO_CACHE;
  }
}

template <typename T>
bool CACHE::should_activate_prefetcher(const T& pkt) const
{
  return !pkt.prefetch_from_this && std::count(std::begin(pref_activate_mask), std::end(pref_activate_mask), pkt.type) > 0;
}

// LCOV_EXCL_START Exclude the following function from LCOV
void CACHE::print_deadlock()
{
  std::string_view mshr_write{"instr_id: {} address: {} v_addr: {} type: {} ready: {}"};
  auto mshr_pack = [time = current_time](const auto& entry) {
    return std::tuple{entry.instr_id, entry.address, entry.v_address, access_type_names.at(champsim::to_underlying(entry.type)),
                      entry.data_promise.is_ready_at(time)};
  };

  std::string_view tag_check_write{"instr_id: {} address: {} v_addr: {} is_translated: {} translate_issued: {} event_cycle: {}"};
  auto tag_check_pack = [period = clock_period](const auto& entry) {
    return std::tuple{entry.instr_id,      entry.address,          entry.v_address,
                      entry.is_translated, entry.translate_issued, entry.event_cycle.time_since_epoch() / period};
  };

  champsim::range_print_deadlock(MSHR, NAME + "_MSHR", mshr_write, mshr_pack);
  champsim::range_print_deadlock(inflight_tag_check, NAME + "_tags", tag_check_write, tag_check_pack);
  champsim::range_print_deadlock(translation_stash, NAME + "_translation", tag_check_write, tag_check_pack);

  std::string_view q_writer{"instr_id: {} address: {} v_addr: {} type: {} translated: {}"};
  auto q_entry_pack = [](const auto& entry) {
    return std::tuple{entry.instr_id, entry.address, entry.v_address, access_type_names.at(champsim::to_underlying(entry.type)), entry.is_translated};
  };

  for (auto* ul : upper_levels) {
    champsim::range_print_deadlock(ul->RQ, NAME + "_RQ", q_writer, q_entry_pack);
    champsim::range_print_deadlock(ul->WQ, NAME + "_WQ", q_writer, q_entry_pack);
    champsim::range_print_deadlock(ul->PQ, NAME + "_PQ", q_writer, q_entry_pack);
  }
}
// LCOV_EXCL_STOP
