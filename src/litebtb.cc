#include "litebtb.h"
#include "storage.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "instruction.h"
#include "ooo_cpu.h"

namespace
{
constexpr std::size_t L1_SETS = 64;
constexpr std::size_t L1_WAYS = 8;
constexpr std::size_t RAS_SIZE = 64;

uint64_t bit_mask(unsigned bits)
{
  return bits >= 64 ? std::numeric_limits<uint64_t>::max() : (bits == 0 ? 0 : (uint64_t{1} << bits) - 1);
}

uint8_t required_target_bits(uint64_t ip, uint64_t target)
{
  uint64_t difference = (ip >> 2) ^ (target >> 2);
  uint8_t result = 0;
  while (difference != 0) {
    ++result;
    difference >>= 1;
  }
  return result;
}

uint64_t reverse_bits(uint64_t value)
{
  uint64_t result = 0;
  while (value != 0) {
    result |= value & 1;
    result <<= 1;
    value >>= 1;
  }
  return result;
}

uint64_t half_xor(uint64_t value, unsigned bits)
{
  if (bits == 0)
    return 0;
  const unsigned lower_bits = bits / 2;
  const unsigned upper_bits = bits - lower_bits;
  const uint64_t lower = value & bit_mask(lower_bits);
  value >>= lower_bits;
  uint64_t upper = 0;
  while (value != 0) {
    upper ^= value & bit_mask(upper_bits);
    value >>= upper_bits;
  }
  return lower | ((upper & bit_mask(upper_bits)) << lower_bits);
}

uint64_t full_xor(uint64_t value, unsigned bits)
{
  if (bits == 0)
    return 0;
  uint64_t result = 0;
  while (value != 0) {
    result ^= value & bit_mask(bits);
    value >>= bits;
  }
  return result & bit_mask(bits);
}

uint64_t geometric(uint64_t value, unsigned bits)
{
  if (bits == 0)
    return 0;
  uint64_t upper = reverse_bits(value >> bits);
  uint64_t result = 0;
  constexpr double ratio = 1.1;
  for (unsigned i = 1; i <= bits; ++i) {
    const auto group_bits = static_cast<unsigned>(std::round((1.0 - std::pow(ratio, i)) / (1.0 - ratio))) - 1;
    uint8_t output_bit = value & 1;
    value >>= 1;
    uint64_t group = upper & bit_mask(group_bits);
    upper >>= std::min(group_bits, 63U);
    while (group != 0) {
      output_bit ^= group & 1;
      group >>= 1;
    }
    // The tagged implementation writes bit i (rather than i-1); retain it.
    if (i < 64)
      result |= uint64_t{output_bit} << i;
  }
  return result & bit_mask(bits);
}

uint64_t two_level_geometric(uint64_t value, unsigned bits)
{
  if (bits == 0)
    return 0;
  uint64_t upper = reverse_bits(value >> bits);
  uint64_t result = value & 1;
  value >>= 1;
  constexpr double ratio = 1.1;
  for (unsigned i = 1; i <= bits; ++i) {
    const auto group_bits = static_cast<unsigned>(std::round(std::pow(ratio, i)));
    uint8_t output_bit = value & 1;
    value >>= 1;
    uint64_t group = upper & bit_mask(group_bits);
    upper >>= group_bits;
    while (group != 0) {
      output_bit ^= group & 1;
      group >>= 1;
    }
    if (i < 64)
      result |= uint64_t{output_bit} << i;
  }
  return result & bit_mask(bits);
}

uint64_t splitmix64(uint64_t& state)
{
  uint64_t value = (state += 0x9e3779b97f4a7c15ULL);
  value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31);
}

uint64_t zobrist(uint64_t value)
{
  static const auto table = [] {
    std::array<uint64_t, 64> result{};
    uint64_t seed = 0xfeedbeef12345678ULL;
    for (auto& elem : result)
      elem = splitmix64(seed);
    return result;
  }();
  uint64_t result = 0;
  for (unsigned bit = 0; bit < 64; ++bit)
    if ((value >> bit) & 1)
      result ^= table[bit];
  return result;
}

uint64_t pearson(uint64_t value)
{
  // The permutation is the one embedded in the MICRO'26 implementation.
  static constexpr std::array<uint8_t, 256> table = {
      98,6,85,150,36,23,112,164,135,207,169,5,26,64,165,219,61,20,68,89,130,63,52,102,24,229,132,245,80,216,195,115,
      90,168,156,203,177,120,2,190,188,7,100,185,174,243,162,10,237,18,253,225,8,208,172,244,255,126,101,79,145,235,228,121,
      123,251,67,250,161,0,107,97,241,111,181,82,249,33,69,55,59,153,29,9,213,167,84,93,30,46,94,75,151,114,73,222,
      197,96,210,45,16,227,248,202,51,152,252,125,81,206,215,186,39,158,178,187,131,136,1,49,50,17,141,91,47,129,60,99,
      154,35,86,171,105,34,38,200,147,58,77,118,126,255,216,83,44,200,240,169,145,36,66,139,213,146,63,157,228,247,152,2,
      14,19,215,68,219,91,176,11,13,214,140,75,78,232,121,37,64,70,227,180,231,92,11,242,31,223,216,104,71,54,41,170,
      250,133,210,24,183,97,56,138,143,199,226,12,106,21,119,148,152,61,236,28,144,10,183,55,232,109,101,240,44,131,148,177,
      96,188,52,27,84,126,3,34,43,117,158,47,222,134,25,46,247,91,64,39,139,216,67,229,15,231,20,189,121,179,73,213};
  uint64_t result = 0;
  for (unsigned output = 0; output < 8; ++output) {
    uint8_t hash = table[output];
    for (unsigned byte = 0; byte < 8; ++byte)
      hash = table[hash ^ static_cast<uint8_t>(value >> (byte * 8))];
    result |= uint64_t{hash} << (output * 8);
  }
  return result;
}

uint64_t rotl64(uint64_t value, unsigned shift) { return shift == 0 ? value : (value << shift) | (value >> (64 - shift)); }

uint64_t sha3(uint64_t value)
{
  static constexpr std::array<uint64_t, 24> constants = {
      0x1ULL,0x8082ULL,0x800000000000808aULL,0x8000000080008000ULL,0x808bULL,0x80000001ULL,
      0x8000000080008081ULL,0x8000000000008009ULL,0x8aULL,0x88ULL,0x80008009ULL,0x8000000aULL,
      0x8000808bULL,0x800000000000008bULL,0x8000000000008089ULL,0x8000000000008003ULL,
      0x8000000000008002ULL,0x8000000000000080ULL,0x800aULL,0x800000008000000aULL,
      0x8000000080008081ULL,0x8000000000008080ULL,0x80000001ULL,0x8000000080008008ULL};
  static constexpr std::array<unsigned, 25> rotations = {0,1,62,28,27,36,44,6,55,20,3,10,43,25,39,41,45,15,21,8,18,2,61,56,14};
  std::array<uint64_t, 25> state{};
  state[0] = value;
  state[1] = 0x06;
  state[16] ^= uint64_t{0x80} << 56; // byte 135 of the SHA3-256 rate block
  for (auto constant : constants) {
    std::array<uint64_t, 5> column{}, delta{};
    for (std::size_t x = 0; x < 5; ++x)
      column[x] = state[x] ^ state[x + 5] ^ state[x + 10] ^ state[x + 15] ^ state[x + 20];
    for (std::size_t x = 0; x < 5; ++x)
      delta[x] = column[(x + 4) % 5] ^ rotl64(column[(x + 1) % 5], 1);
    for (std::size_t y = 0; y < 5; ++y)
      for (std::size_t x = 0; x < 5; ++x)
        state[x + 5 * y] ^= delta[x];
    std::array<uint64_t, 25> rotated{};
    for (std::size_t y = 0; y < 5; ++y)
      for (std::size_t x = 0; x < 5; ++x)
        rotated[y + 5 * ((2 * x + 3 * y) % 5)] = rotl64(state[x + 5 * y], rotations[x + 5 * y]);
    for (std::size_t y = 0; y < 5; ++y)
      for (std::size_t x = 0; x < 5; ++x)
        state[x + 5 * y] = rotated[x + 5 * y] ^ ((~rotated[(x + 1) % 5 + 5 * y]) & rotated[(x + 2) % 5 + 5 * y]);
    state[0] ^= constant;
  }
  return state[0];
}
} // namespace

litebtb_predictor::litebtb_predictor(O3_CPU* cpu, litebtb_hash hash, bool enable_two_level)
    : btb(cpu), hash_kind(hash), two_level(enable_two_level), sets(cpu->BTB_SETS), ways(cpu->BTB_WAYS), clipped_tag(cpu->BTB_CLIPPED_TAG),
      tag_size(cpu->BTB_TAG_SIZE.value_or(12)), full_target_tag_size(cpu->BTB_FULL_TARGET_TAG_SIZE),
      target_sizes(cpu->BTB_TARGET_SIZES), address_permutation(cpu->BTB_INDEX_TAG_HASH)
{
  if (sets == 0 || (sets & (sets - 1)) != 0)
    throw std::invalid_argument("LiteBTB sets must be a nonzero power of two");
  if (ways == 0)
    throw std::invalid_argument("LiteBTB ways must be nonzero");
  if (tag_size > 63)
    throw std::invalid_argument("LiteBTB tag size must not exceed 63 bits");
  if (full_target_tag_size && (*full_target_tag_size == 0 || *full_target_tag_size > 62 - champsim::storage_bits(sets)))
    throw std::invalid_argument("LiteBTB full-target tag size must be positive and no larger than the full tag");
  if (cpu->BTB_TAG_REGIONS != 0)
    throw std::invalid_argument("LiteBTB uses btb_utb_entries/btb_utb_ways rather than legacy tag regions");
  if (cpu->BTB_UTB_REPLACEMENT != "lru" && cpu->BTB_UTB_REPLACEMENT != "reference_count")
    throw std::invalid_argument("LiteBTB UTB replacement must be lru or reference_count");
  utb_reference_count = cpu->BTB_UTB_REPLACEMENT == "reference_count";
  utb_ways = cpu->BTB_UTB_WAYS;
  if (cpu->BTB_UTB_ENTRIES != 0) {
    if (!clipped_tag)
      throw std::invalid_argument("LiteBTB UTB requires clipped tags");
    if (utb_ways == 0 || cpu->BTB_UTB_ENTRIES % utb_ways != 0)
      throw std::invalid_argument("LiteBTB UTB entries must be divisible by nonzero UTB ways");
    const auto utb_sets = cpu->BTB_UTB_ENTRIES / utb_ways;
    if ((utb_sets & (utb_sets - 1)) != 0 || champsim::storage_bits(utb_sets) > tag_size)
      throw std::invalid_argument("LiteBTB UTB sets must be a power of two fitting in the tag");
    utb.resize(cpu->BTB_UTB_ENTRIES);
  } else if (utb_ways != 0) {
    throw std::invalid_argument("LiteBTB UTB ways require nonzero UTB entries");
  }
  if (target_sizes.empty())
    target_sizes.assign(ways, 64);
  if (target_sizes.size() != ways)
    throw std::invalid_argument("LiteBTB target-size list must have one entry per way");
  if (!std::is_sorted(target_sizes.begin(), target_sizes.end()) || target_sizes.back() > 64)
    throw std::invalid_argument("LiteBTB target sizes must be sorted and at most 64 bits");
  entries.resize(sets * ways);
  for (std::size_t set = 0; set < sets; ++set)
    for (std::size_t way = 0; way < ways; ++way)
      entries[set * ways + way].target_size = target_sizes[way];
  if (two_level)
    l1_entries.resize(L1_SETS * L1_WAYS);
}

uint64_t litebtb_predictor::permute(uint64_t value) const
{
  if (address_permutation.empty())
    return value;
  uint64_t result = 0;
  std::size_t bit = 0;
  for (; bit < address_permutation.size() && bit < 64; ++bit) {
    if (address_permutation[bit] >= 64)
      throw std::invalid_argument("LiteBTB address permutation contains an invalid bit");
    result |= ((value >> address_permutation[bit]) & 1) << bit;
  }
  for (; bit < 64; ++bit)
    result |= value & (uint64_t{1} << bit);
  return result;
}

std::size_t litebtb_predictor::index(uint64_t ip, std::size_t table_sets) const
{
  const bool shuffled = two_level || hash_kind == litebtb_hash::full_xor || hash_kind == litebtb_hash::geometric;
  return static_cast<std::size_t>(((shuffled ? permute(ip) : ip) >> 2) & (table_sets - 1));
}

uint8_t litebtb_predictor::tag_bits(uint8_t capacity) const
{
  const auto full_width = static_cast<uint8_t>(62 - champsim::storage_bits(sets));
  if (!clipped_tag)
    return full_width;
  if (capacity == 64) {
    if (full_target_tag_size)
      return *full_target_tag_size;
    if (!two_level && hash_kind != litebtb_hash::full_xor && hash_kind != litebtb_hash::geometric)
      return full_width;
  }
  return tag_size;
}

uint64_t litebtb_predictor::tag(uint64_t ip, uint8_t capacity) const
{
  const bool shuffled = two_level || hash_kind == litebtb_hash::full_xor || hash_kind == litebtb_hash::geometric;
  const unsigned set_bits = static_cast<unsigned>(std::log2(sets));
  uint64_t value = (shuffled ? permute(ip) : ip) >> (2 + set_bits);
  const auto width = tag_bits(capacity);
  const bool full_way_bypasses_hash = !two_level && hash_kind != litebtb_hash::full_xor && hash_kind != litebtb_hash::geometric;
  if (!clipped_tag || (capacity == 64 && full_way_bypasses_hash))
    return value & bit_mask(width);
  switch (hash_kind) {
  case litebtb_hash::identity: return value & bit_mask(width);
  case litebtb_hash::half_xor: return half_xor(value, width);
  case litebtb_hash::full_xor: return full_xor(value, width);
  case litebtb_hash::geometric: return two_level ? two_level_geometric(value, width) : geometric(value, width);
  case litebtb_hash::pearson: return pearson(value) & bit_mask(width);
  // The tagged Rabin routine reduces a 64-bit value with a degree-64 polynomial;
  // for a single 64-bit input this is the identity before clipping.
  case litebtb_hash::rabin: return value & bit_mask(width);
  case litebtb_hash::sha3: return sha3(value) & bit_mask(width);
  case litebtb_hash::zobrist: return zobrist(value) & bit_mask(width);
  }
  return value;
}

uint64_t litebtb_predictor::decompress(const entry& value) const
{
  const uint64_t mask = bit_mask(std::min<unsigned>(64, value.target_size + 2));
  return (value.ip & ~mask) | (value.target & mask);
}

bool litebtb_predictor::uses_utb(uint8_t capacity) const
{
  // The artifact's optional full-target way retains its inline tag.
  return !utb.empty() && capacity != 64;
}

std::optional<std::size_t> litebtb_predictor::find_utb(uint64_t ip, uint8_t capacity) const
{
  const auto key = tag(ip, capacity);
  const auto utb_sets = utb.size() / utb_ways;
  const auto first = static_cast<std::size_t>(key & (utb_sets - 1)) * utb_ways;
  for (auto slot = first; slot < first + utb_ways; ++slot)
    if (utb[slot].generation != 0 && utb[slot].tag == key)
      return slot;
  return std::nullopt;
}

std::size_t litebtb_predictor::allocate_utb(uint64_t ip, uint8_t capacity)
{
  if (auto hit = find_utb(ip, capacity); hit) {
    utb[*hit].last_used = ++access_count;
    return *hit;
  }
  const auto key = tag(ip, capacity);
  const auto utb_sets = utb.size() / utb_ways;
  const auto first = static_cast<std::size_t>(key & (utb_sets - 1)) * utb_ways;
  auto selected = first;
  for (auto slot = first; slot < first + utb_ways; ++slot) {
    if (utb[slot].generation == 0) {
      selected = slot;
      break;
    }
    if ((utb_reference_count && utb[slot].references < utb[selected].references)
        || ((!utb_reference_count || utb[slot].references == utb[selected].references) && utb[slot].last_used < utb[selected].last_used))
      selected = slot;
  }
  // Stale BTB pointers survive eviction, but do not belong to the new tag's
  // counter. The inserting BTB entry will take this count from zero to one.
  utb[selected] = utb_entry{key, ++access_count, 0, ++next_generation};
  return selected;
}

bool litebtb_predictor::matches(const entry& value, uint64_t ip, bool compressed, std::optional<std::size_t> pointer) const
{
  if (value.last_used == 0)
    return false;
  if (!compressed)
    return (permute(value.ip) >> 2) == (permute(ip) >> 2);
  if (uses_utb(value.target_size)) {
    // Deliberately do not compare generations here: stale pointers can alias
    // after UTB replacement, and can become useful again on tag reinsertion.
    return pointer && pointer == value.utb_pointer;
  }
  return tag(value.ip, value.target_size) == tag(ip, value.target_size);
}

void litebtb_predictor::invalidate(entry& value)
{
  if (value.last_used != 0 && value.utb_pointer) {
    auto& owner = utb[*value.utb_pointer];
    if (owner.generation == value.utb_generation) {
      assert(owner.references != 0);
      --owner.references;
    }
  }
  value.last_used = 0;
  value.utb_pointer.reset();
}

std::vector<std::size_t> litebtb_predictor::utb_reference_counts() const
{
  std::vector<std::size_t> result;
  for (const auto& value : utb)
    result.push_back(value.references);
  return result;
}

litebtb_predictor::entry* litebtb_predictor::find(std::vector<entry>& table, std::size_t table_sets, std::size_t table_ways, uint64_t ip,
                                                  bool compressed)
{
  const auto first = table.begin() + static_cast<std::ptrdiff_t>(index(ip, table_sets) * table_ways);
  const auto pointer = compressed && !utb.empty() ? find_utb(ip, 0) : std::nullopt;
  for (auto it = first; it != first + static_cast<std::ptrdiff_t>(table_ways); ++it) {
    if (matches(*it, ip, compressed, pointer)) {
      it->last_used = ++access_count;
      return &*it;
    }
  }
  return nullptr;
}

void litebtb_predictor::fill(std::vector<entry>& table, std::size_t table_sets, std::size_t table_ways, uint64_t ip, uint64_t target,
                             uint8_t precise_type, uint8_t required_bits, bool compressed)
{
  auto first = table.begin() + static_cast<std::ptrdiff_t>(index(ip, table_sets) * table_ways);
  auto last = first + static_cast<std::ptrdiff_t>(table_ways);
  auto pointer = compressed && !utb.empty() ? find_utb(ip, 0) : std::nullopt;
  entry* selected = nullptr;
  // A target that outgrows its current way must not leave a stale, earlier
  // match behind after it is migrated to a wider way.
  if (compressed) {
    for (auto it = first; it != last; ++it)
      if (it->target_size < required_bits && matches(*it, ip, true, pointer))
        invalidate(*it);
  }
  for (auto it = first; it != last; ++it) {
    if (it->target_size < required_bits)
      continue;
    const bool match = matches(*it, ip, compressed, pointer);
    if (match) {
      selected = &*it;
      break;
    }
    if (selected == nullptr || (selected->last_used != 0 && it->last_used == 0) ||
        (selected->last_used == 0 && it->last_used == 0 && it->target_size < selected->target_size) ||
        (selected->last_used != 0 && it->last_used != 0 && it->last_used < selected->last_used))
      selected = &*it;
  }
  if (selected == nullptr)
    throw std::invalid_argument("LiteBTB has no way large enough for this target");
  if (compressed && uses_utb(selected->target_size)) {
    pointer = allocate_utb(ip, selected->target_size);
    // Allocation can make a retained stale pointer match the new tag.
    for (auto it = first; it != last; ++it) {
      if (it->target_size < required_bits && matches(*it, ip, true, pointer))
        invalidate(*it);
    }
    for (auto it = first; it != last; ++it)
      if (it->target_size >= required_bits && matches(*it, ip, true, pointer)) {
        selected = &*it;
        break;
      }
  }
  const auto capacity = selected->target_size;
  invalidate(*selected);
  *selected = entry{ip, target, ++access_count, capacity, precise_type, branch_info::always_taken, std::nullopt, 0, 0};
  selected->generation = ++next_generation;
  if (compressed && uses_utb(capacity)) {
    selected->utb_pointer = pointer;
    auto& owner = utb[*selected->utb_pointer];
    selected->utb_generation = owner.generation;
    ++owner.references;
  }
  if (precise_type == BRANCH_INDIRECT || precise_type == BRANCH_INDIRECT_CALL)
    selected->type = branch_info::indirect;
  else if (precise_type == BRANCH_RETURN)
    selected->type = branch_info::return_branch;
  else if (precise_type == BRANCH_CONDITIONAL || precise_type == BRANCH_OTHER)
    selected->type = branch_info::conditional;
}

std::pair<champsim::address, bool> litebtb_predictor::btb_prediction(champsim::address address)
{
  intern_->prediction_btb_token.reset();
  intern_->prediction_btb_hit = false;
  const uint64_t ip = address.to<uint64_t>();
  entry* hit = two_level ? find(l1_entries, L1_SETS, L1_WAYS, ip, false) : nullptr;
  const bool in_l1 = hit != nullptr;
  if (hit == nullptr) {
    if (!utb.empty())
      if (const auto pointer = find_utb(ip, 0); pointer)
        utb[*pointer].last_used = ++access_count;
    hit = find(entries, sets, ways, ip, true);
  }
  if (hit == nullptr)
    return {champsim::address{}, false};
  intern_->prediction_btb_hit = true;
  if (!utb.empty()) {
    const auto slot = in_l1 ? entries.size() + static_cast<std::size_t>(hit - l1_entries.data()) : static_cast<std::size_t>(hit - entries.data());
    intern_->prediction_btb_token = champsim::btb_prediction_token{slot, hit->generation, this};
  }
  if (hit->type == branch_info::return_branch) {
    const uint64_t target = ras.empty() ? 0 : ras.back() + 4;
    return {champsim::address{target}, true};
  }
  if (hit->type == branch_info::indirect)
    return indirect.prediction(address);
  const auto mask = bit_mask(std::min<unsigned>(64, hit->target_size + 2));
  const auto target = hit->utb_pointer ? (ip & ~mask) | (hit->target & mask) : decompress(*hit);
  return {champsim::address{target}, hit->type != branch_info::conditional};
}

void litebtb_predictor::btb_decode(champsim::btb_prediction_token token, uint8_t branch_type)
{
  if (token.owner != this || token.slot >= entries.size() + l1_entries.size())
    return;
  auto& value = token.slot < entries.size() ? entries[token.slot] : l1_entries[token.slot - entries.size()];
  // A later fill may already have replaced or repaired the predicted entry.
  if (value.last_used != 0 && value.generation == token.generation && (branch_type == NOT_BRANCH || value.precise_type != branch_type))
    invalidate(value);
}

void litebtb_predictor::update_btb(champsim::address ip_address, champsim::address target_address, bool taken, uint8_t branch_type)
{
  const uint64_t ip = ip_address.to<uint64_t>();
  const uint64_t target = target_address.to<uint64_t>();
  indirect.update(ip_address, target_address, taken, branch_type);
  if (branch_type == BRANCH_DIRECT_CALL || branch_type == BRANCH_INDIRECT_CALL) {
    ras.push_back(ip);
    if (ras.size() > RAS_SIZE)
      ras.pop_front();
  } else if (branch_type == BRANCH_RETURN && !ras.empty()) {
    ras.pop_back();
  }
  if (target == 0)
    return;
  const uint8_t bits = branch_type == BRANCH_RETURN ? 0 : required_target_bits(ip, target);
  fill(entries, sets, ways, ip, target, branch_type, bits, true);
  if (two_level)
    fill(l1_entries, L1_SETS, L1_WAYS, ip, target, branch_type, 0, false);
}

void litebtb_predictor::initialize_btb()
{
  uint64_t targets = 0, tags = 0, pointers = 0;
  for (auto capacity : target_sizes) {
    targets += sets * capacity;
    if (uses_utb(capacity))
      pointers += sets * champsim::storage_bits(utb.size());
    else
      tags += sets * tag_bits(capacity);
  }
  const uint64_t count = sets * ways;
  const uint64_t l1_count = two_level ? L1_SETS * L1_WAYS : 0;
  const uint64_t utb_tags = utb.empty() ? 0 : utb.size() * (tag_size - champsim::storage_bits(utb.size() / utb_ways));
  champsim::print_storage("CPU " + std::to_string(intern_->cpu) + " LiteBTB hash=" + std::to_string(static_cast<int>(hash_kind))
                             + (two_level ? " two-level" : "") + " UTB=" + std::to_string(utb.size())
                             + " replacement=" + (utb_reference_count ? "reference_count" : "lru"), {
      {"UTB upper tags", utb_tags},
      {"UTB valid bits", utb.size()},
      {"UTB LRU ranks (also break reference-count ties)", utb.size() * champsim::storage_bits(utb_ways)},
      {"UTB reference counters", utb_reference_count ? utb.size() * champsim::storage_bits(count + 1) : 0},
      {"Tag pointers", pointers},
      {"Targets", targets}, {"Tags", tags}, {"Valid bits", count}, {"Branch types", count * 2},
      {"Replacement (LRU ranks)", count * champsim::storage_bits(ways)},
      {"L1 BTB targets", l1_count * 64},
      {"L1 BTB tags", l1_count * (64 - 2 - champsim::storage_bits(L1_SETS))},
      {"L1 BTB valid/type/LRU metadata", l1_count * (1 + 2 + champsim::storage_bits(L1_WAYS))},
      {"ITTAGE base targets/valid", ittage::base_storage_bits},
      {"ITTAGE tagged targets/tags/confidence/useful/valid", ittage::tagged_storage_bits},
      {"ITTAGE history/folds/cursor/chooser/aging", ittage::history_storage_bits},
      {"Return address stack", RAS_SIZE * 64}, {"Return stack pointer", champsim::storage_bits(RAS_SIZE + 1)}});
}
