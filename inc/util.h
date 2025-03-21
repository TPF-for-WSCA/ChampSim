#ifndef UTIL_H
#define UTIL_H

#include <cstdint>
#include <iostream>
#include <typeinfo>

constexpr unsigned lg2(uint64_t n) { return n < 2 ? 0 : 1 + lg2(n / 2); }

constexpr uint64_t bitmask(std::size_t begin, std::size_t end = 0) { return ((1ull << (begin - end)) - 1) << end; }

constexpr uint64_t splice_bits(uint64_t upper, uint64_t lower, std::size_t bits) { return (upper & ~bitmask(bits)) | (lower & bitmask(bits)); }

template <typename T>
struct is_valid {
  using argument_type = T;
  size_t size;
  is_valid() { size = 0; };
  bool operator()(const argument_type& test) { return test.valid; }
};

template <typename T>
struct is_valid_size {
  using argument_type = T;
  size_t size;
  is_valid_size(size_t S) { size = S; };
  bool operator()(const argument_type& test)
  {
    if (size <= test.size && !test.valid)
      return false; // it fits and it is not valid
    return true;
  }
};
template <typename T>
struct eq_addr {
  using argument_type = T;
  const decltype(argument_type::address) val;
  const std::size_t shamt = 0;

  explicit eq_addr(decltype(argument_type::address) val) : val(val) {}
  eq_addr(decltype(argument_type::address) val, std::size_t shamt) : val(val), shamt(shamt) {}

  bool operator()(const argument_type& test)
  {
    is_valid<argument_type> validtest;
    return validtest(test) && (test.address >> shamt) == (val >> shamt);
  }
};

template <typename T>
struct eq_v_addr {
  using argument_type = T;
  const decltype(argument_type::v_address) val;
  const std::size_t shamt = 0;

  explicit eq_v_addr(decltype(argument_type::v_address) val) : val(val) {}
  eq_v_addr(decltype(argument_type::v_address) val, std::size_t shamt) : val(val), shamt(shamt) {}

  bool operator()(const argument_type& test)
  {
    is_valid<argument_type> validtest;
    return validtest(test) && (test.v_address >> shamt) == (val >> shamt);
  }
};

template <typename T>
struct eq_vcl_addr {
  using argument_type = T;
  const decltype(argument_type::address) val;
  const decltype(argument_type::v_address) offset;
  const decltype(argument_type::size) size;
  const std::size_t shamt = 0;

  explicit eq_vcl_addr(decltype(argument_type::address) val, decltype(argument_type::v_address) offset, decltype(argument_type::size) size)
      : val(val), offset(offset), size(size)
  {
  }
  eq_vcl_addr(decltype(argument_type::address) val, decltype(argument_type::v_address) offset, decltype(argument_type::size) size, std::size_t shamt)
      : val(val), offset(offset), size(size), shamt(shamt)
  {
  }

  bool operator()(const argument_type& test)
  {
    is_valid<argument_type> validtest;
    if (!validtest(test)) {
      return false;
    }
    auto testoffset = test.v_address & 63;
    auto testendoffset = testoffset + test.size;
    bool same_base = (test.address >> shamt) == (val >> shamt);
    bool start_within_range = testoffset <= offset && offset < testendoffset;
    return same_base && start_within_range;
  }
};

template <typename T, typename BIN, typename U = T, typename UN_T = is_valid<T>, typename UN_U = is_valid<U>>
struct invalid_is_minimal {
  bool operator()(const T& lhs, const U& rhs)
  {
    UN_T lhs_unary;
    UN_U rhs_unary;
    BIN cmp;

    return !lhs_unary(lhs) || (rhs_unary(rhs) && cmp(lhs, rhs));
  }
};

template <typename T, typename BIN, typename U = T, typename UN_T = is_valid<T>, typename UN_U = is_valid<U>>
struct invalid_is_maximal {
  bool operator()(const T& lhs, const U& rhs)
  {
    UN_T lhs_unary;
    UN_U rhs_unary;
    BIN cmp;

    return !rhs_unary(rhs) || (lhs_unary(lhs) && cmp(lhs, rhs));
  }
};

template <typename T, typename U = T>
struct cmp_event_cycle {
  bool operator()(const T& lhs, const U& rhs) { return lhs.event_cycle < rhs.event_cycle; }
};

template <typename T>
struct min_event_cycle : invalid_is_maximal<T, cmp_event_cycle<T>> {
};

template <typename T, typename U = T>
struct cmp_lru {
  bool operator()(const T& lhs, const U& rhs) { return lhs.lru < rhs.lru; }
};

template <typename T, typename U = T>
struct cmp_mti {
  bool operator()(const T& lhs, const U& rhs) { return lhs.max_time < rhs.max_time; }
};

/*
 * A comparator to determine the LRU element. To use this comparator, the type
 * must have a member variable named "lru" and have a specialization of
 * is_valid<>.
 *
 * To use:
 *     auto lru_elem = std::max_element(std::begin(set), std::end(set),
 * lru_comparator<BLOCK>());
 *
 * The MRU element can be found using std::min_element instead.
 */
template <typename T, typename U = T>
struct lru_comparator : invalid_is_maximal<T, cmp_lru<T, U>, U> {
  using first_argument_type = T;
  using second_argument_type = U;
};

template <typename T, typename U = T>
struct max_time_comparator : invalid_is_maximal<T, cmp_mti<T, U>, U> {
  using first_argument_type = T;
  using second_argument_type = U;
};

/*
 * A functor to reorder elements to a new LRU order.
 * The type must have a member variable named "lru".
 *
 * To use:
 *     std::for_each(std::begin(set), std::end(set),
 * lru_updater<BLOCK>(hit_element));
 */
template <typename T>
struct lru_updater {
  const decltype(T::lru) val;
  explicit lru_updater(decltype(T::lru) val) : val(val) {}

  template <typename U>
  explicit lru_updater(U iter) : val(iter->lru)
  {
  }

  void operator()(T& x)
  {
    if (x.lru == val)
      x.lru = 0;
    else
      ++x.lru;
  }
};

template <typename T, typename U = T>
struct ord_event_cycle {
  using first_argument_type = T;
  using second_argument_type = U;
  bool operator()(const first_argument_type& lhs, const second_argument_type& rhs)
  {
    is_valid<first_argument_type> first_validtest;
    is_valid<second_argument_type> second_validtest;
    return !second_validtest(rhs) || (first_validtest(lhs) && lhs.event_cycle < rhs.event_cycle);
  }
};

#endif
