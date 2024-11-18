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

#ifndef MSL_LRU_TABLE_H
#define MSL_LRU_TABLE_H

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

#include "msl/bits.h"

namespace champsim::msl
{
namespace detail
{
template <typename T>
struct table_indexer {
  auto operator()(const T& t) const
  {
    auto ret_val = t.index();
    return ret_val;
  }
};

template <typename T>
struct table_tagger {
  auto operator()(const T& t) const { return t.tag(); }
};

template <typename T>
struct partial_tagger {
  auto operator()(const T& t) const { return t.partial_tag(); }
};
} // namespace detail

template <typename T, typename SetProj = detail::table_indexer<T>, typename TagProj = detail::table_tagger<T>, typename PartTagProj = detail::partial_tagger<T>>
class lru_table
{
public:
  using value_type = T;
  auto get_set_span(uint16_t idx)
  {
    using diff_type = typename block_vec_type::difference_type;
    auto set_begin = std::next(std::begin(block), idx * static_cast<diff_type>(NUM_WAY));
    auto set_end = std::next(set_begin, static_cast<diff_type>(NUM_WAY));
    return std::pair{set_begin, set_end};
  }

private:
  struct block_t {
    uint64_t last_used = 0;
    value_type data;
  };
  using block_vec_type = std::vector<block_t>;

  SetProj set_projection;
  TagProj tag_projection;
  PartTagProj partial_tag_projection;

  std::size_t NUM_SET, NUM_WAY;
  uint64_t access_count = 0;
  block_vec_type block{NUM_SET * NUM_WAY};

  auto get_set_span(const value_type& elem)
  {
    using diff_type = typename block_vec_type::difference_type;
    auto set_idx = static_cast<diff_type>(set_projection(elem) & bitmask(lg2(NUM_SET)));
    auto set_begin = std::next(std::begin(block), set_idx * static_cast<diff_type>(NUM_WAY));
    auto set_end = std::next(set_begin, static_cast<diff_type>(NUM_WAY));
    return std::pair{set_begin, set_end};
  }

  auto get_set_span(const value_type& elem, uint8_t size)
  {
    using diff_type = typename block_vec_type::difference_type;
    auto set_idx = static_cast<diff_type>(set_projection(elem) & bitmask(lg2(NUM_SET)));
    auto set_begin = std::next(std::begin(block), set_idx * static_cast<diff_type>(NUM_WAY));
    auto set_end = std::next(set_begin, static_cast<diff_type>(NUM_WAY));
    while (set_begin->target_size < size) {
      set_begin++;
    }
    return std::pair{set_begin, set_end};
  }

  auto partial_match(const value_type& elem)
  {
    return [partial_tag = partial_tag_projection(elem), partial_proj = this->partial_tag_projection](const block_t& x) {
      return x.last_used > 0 && partial_proj(x.data) == partial_tag;
    };
  }

  auto match_func(const value_type& elem)
  {
    return [tag = tag_projection(elem), proj = this->tag_projection](const block_t& x) {
      return x.last_used > 0 && proj(x.data) == tag;
    };
  }

  template <typename U>
  auto match_and_check(U tag)
  {
    return [tag, proj = this->tag_projection](const auto& x, const auto& y) {
      auto x_valid = x.last_used > 0;
      auto y_valid = y.last_used > 0;
      auto x_match = proj(x.data) == tag;
      auto y_match = proj(y.data) == tag;
      auto cmp_lru = x.last_used < y.last_used;
      return !x_valid || (y_valid && ((!x_match && y_match) || ((x_match == y_match) && cmp_lru)));
    };
  }

public:
  std::optional<value_type> check_hit(const value_type& elem, bool partial = false)
  {
    auto [set_begin, set_end] = get_set_span(elem);
    typename std::vector<block_t>::iterator hit;
    if (partial) {
      hit = std::find_if(set_begin, set_end, partial_match(elem));
    } else {
      hit = std::find_if(set_begin, set_end, match_func(elem));
    }
    if (hit == set_end)
      return std::nullopt;

    hit->last_used = ++access_count;
    return hit->data;
  }
  /*
    std::optional<value_type> check_hit(const value_type& elem)
    {
      auto [set_begin, set_end] = get_set_span(elem);
      auto hit = std::find_if(set_begin, set_end, match_func(elem));

      if (hit == set_end)
        return std::nullopt;

      hit->last_used = ++access_count;
      return hit->data;
    }
    */

  std::optional<uint8_t> check_hit_idx(const value_type& elem)
  {
    auto [set_begin, set_end] = get_set_span(elem);
    auto hit = std::find_if(set_begin, set_end, match_func(elem));

    if (hit == set_end)
      return std::nullopt;

    hit->last_used = ++access_count;
    return hit - set_begin;
  }

  std::optional<value_type> fill(const value_type& elem, uint8_t size)
  {
    auto tag = tag_projection(elem);
    auto [set_begin, set_end] = get_set_span(elem);
    while (set_begin->data.target_size < size && set_begin != set_end) {
      set_begin++;
    }
    if (set_begin != set_end) {
      auto [miss, hit] = std::minmax_element(set_begin, set_end, match_and_check(tag));
      if (tag_projection(hit->data) == tag) {
        auto target_size = hit->data.target_size;
        auto offset_mask = hit->data.offset_mask;
        *hit = {++access_count, elem};
        hit->data.target_size = target_size;
        hit->data.offset_mask = offset_mask;
        return std::nullopt;
      } else {
        std::optional<value_type> replaced = (miss->last_used > 0) ? std::optional<value_type>{miss->data} : std::nullopt;
        auto target_size = miss->data.target_size;
        auto offset_mask = miss->data.offset_mask;
        *miss = {++access_count, elem};
        miss->data.target_size = target_size;
        miss->data.offset_mask = offset_mask;
        return replaced;
      }
    }
    return std::nullopt;
  }

  std::optional<value_type> fill(const value_type& elem)
  {
    auto tag = tag_projection(elem);
    auto [set_begin, set_end] = get_set_span(elem);
    if (set_begin != set_end) {
      auto [miss, hit] = std::minmax_element(set_begin, set_end, match_and_check(tag));

      if (tag_projection(hit->data) == tag) {
        *hit = {++access_count, elem};
        return std::nullopt;
      } else {
        std::optional<value_type> rv = (miss->last_used > 0) ? std::optional<value_type>{miss->data} : std::nullopt;
        *miss = {++access_count, elem};
        return rv;
      }
    }
    return std::nullopt;
  }

  void invalidate_region(const value_type& elem)
  {
    for (auto entry = std::begin(block); entry != std::end(block); entry++) {
      if (entry->data.offset_tag == elem.offset_tag) {
        std::exchange(*entry, {});
      }
    }
  }

  std::optional<value_type> invalidate(const value_type& elem)
  {
    auto [set_begin, set_end] = get_set_span(elem);
    auto hit = std::find_if(set_begin, set_end, match_func(elem));

    if (hit == set_end)
      return std::nullopt;

    return std::exchange(*hit, {}).data;
  }

  lru_table(std::size_t sets, std::size_t ways, SetProj set_proj, TagProj tag_proj)
      : set_projection(set_proj), tag_projection(tag_proj), NUM_SET(sets), NUM_WAY(ways)
  {
    assert(sets == 0 || sets == (1ull << lg2(sets)));
  }

  lru_table(std::size_t sets, std::size_t ways, SetProj set_proj) : lru_table(sets, ways, set_proj, {}) {}
  lru_table(std::size_t sets, std::size_t ways) : lru_table(sets, ways, {}, {}) {}
};
} // namespace champsim::msl

#endif
