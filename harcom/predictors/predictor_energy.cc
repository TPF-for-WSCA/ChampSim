// These costs use the hardware geometry and activity of the selected modules.
#undef CHAMPSIM_MODULE
#include "../ittage_cost.h"
#include "../sram_cost.h"
#include "../../branch/bimodal/bimodal.h"
#include "../../branch/hashed_perceptron/hashed_perceptron.h"
#include "../../branch/perceptron/perceptron.h"
#include "ooo_cpu.h"
#include "predictor_energy.h"
#include "storage.h"

#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace champsim
{
namespace
{
constexpr std::string_view revision = "5ceea8358ddbab94e5621e2536530b860cc1e5b7";
constexpr auto xor_bit = hcm::XOR<2>.e;
constexpr auto flop_bit = hcm::flipflops<1>::write_energy_fJ;
constexpr auto generation_bits = std::numeric_limits<uint64_t>::digits;
using harcom::access_cost;
using harcom::match_energy;
using harcom::sram_row;

double mux_energy(std::size_t inputs, std::size_t width)
{
  if (inputs <= 1 || width == 0)
    return 0;
  const auto mux = hcm::mux(inputs, width, hcm::OUTCAP);
  return mux[0].e + mux[1].e;
}

double select_way(std::size_t ways)
{
  return hcm::oring(ways, hcm::OUTCAP).e + static_cast<double>(ways - 1) * hcm::AND<2>.e;
}

double increment_energy(std::size_t bits)
{
  return bits == 0 ? 0 : hcm::adder_ks<true, true>(bits, hcm::OUTCAP).e;
}

double rank_update(std::size_t ways)
{
  const auto bits = storage_bits(ways);
  if (bits == 0)
    return 0;
  const auto per_way = hcm::unsigned_greater_than(bits, hcm::OUTCAP).e + increment_energy(bits) + mux_energy(2, bits);
  return static_cast<double>(ways) * per_way;
}

template <std::size_t Modulus>
double constant_remainder()
{
  // An unrolled fixed-modulus reduction. Each stage consumes one address bit
  // and keeps the remainder using constant compare and subtract.
  constexpr auto width = champsim::lg2(Modulus) + 2;
  return (66 - width) * (hcm::GTE<width>.e + hcm::SUB<width>.e + mux_energy(2, width - 1));
}

template <std::size_t Width>
double saturating_update()
{
  return hcm::INC<Width>.e + hcm::SUB<Width>.e + 2 * hcm::EQUAL<Width>.e + mux_energy(2, Width);
}

struct direction_cost {
  std::string_view name;
  std::string_view assumptions;
  double lookup{};
  double resolve{};
  double train{};
  double threshold_step{};
};

direction_cost bimodal_cost()
{
  using table = hcm::sram<bimodal::TABLE_SIZE, bimodal::BITS>;
  static_assert(table::ok);
  const auto read = constant_remainder<bimodal::PRIME>() + table::EREAD;
  return {"bimodal", "16,384 two-bit SRAM counters, constant modulo-16,381 index and saturating read-modify-write",
          read, read + table::EWRITE + saturating_update<bimodal::BITS>()};
}

direction_cost perceptron_cost()
{
  constexpr auto history = perceptron::PERCEPTRON_HISTORY;
  constexpr auto weight = perceptron::PERCEPTRON_BITS;
  constexpr auto sum = weight + champsim::lg2(history) + 1;
  using table = hcm::sram<perceptron::NUM_PERCEPTRONS, (history + 1) * weight>;
  static_assert(table::ok);
  const auto lookup = constant_remainder<perceptron::NUM_PERCEPTRONS>() + table::EREAD
      + history * (hcm::ADD<sum>.e + hcm::SUB<sum>.e + mux_energy(2, sum));
  return {"perceptron", "163 SRAM rows of 25 signed weights, 24-bit history and signed dot product; training recomputes the score",
          lookup, lookup + 2 * hcm::GTE<sum>.e + hcm::AND<2>.e + history * flop_bit,
          table::EWRITE + (history + 1) * saturating_update<weight>() + history * xor_bit};
}

direction_cost hashed_perceptron_cost()
{
  constexpr auto banks = hashed_perceptron::NTABLES;
  constexpr auto weight = hashed_perceptron::weight_bits;
  constexpr auto history = static_cast<unsigned>(to_underlying(hashed_perceptron::MAXHIST));
  constexpr auto sum = weight + champsim::lg2(banks);
  using table = hcm::sram<hashed_perceptron::TABLE_SIZE, weight>;
  static_assert(table::ok);
  // Each history bit is XORed into a wired PC bit. All histories shift on
  // the same outcome, so their prefixes can share one register.
  unsigned fold_xors = 0;
  for (const auto length : hashed_perceptron::history_lengths)
    fold_xors += static_cast<unsigned>(to_underlying(length));
  return {"hashed perceptron", "16 SRAM banks of 4,096 signed weights, shared 232-bit history with fixed XOR taps, and adaptive threshold",
          banks * table::EREAD + fold_xors * xor_bit + hcm::ADDN<weight, banks>.e + hcm::GTE<sum>.e,
          history * flop_bit + hcm::SUB<sum + 1>.e + mux_energy(2, sum + 1) + hcm::GTE<32>.e + hcm::AND<2>.e,
          banks * (table::EWRITE + saturating_update<weight>()) + hcm::INC<6>.e + hcm::EQUAL<6>.e + 6 * flop_bit,
          hcm::INC<32>.e + 32 * flop_bit};
}

class direction_model final : public predictor_energy_model
{
  const direction_cost cost;
  double dynamic_energy{};

public:
  explicit direction_model(direction_cost descriptor) : cost(descriptor) {}

  void record(predictor_event event, std::size_t) override
  {
    switch (event) {
    case predictor_event::lookup: dynamic_energy += cost.lookup; break;
    case predictor_event::resolve: dynamic_energy += cost.resolve; break;
    case predictor_event::train: dynamic_energy += cost.train; break;
    case predictor_event::threshold_step: dynamic_energy += cost.threshold_step; break;
    default: break;
    }
  }

  std::vector<energy_model_snapshot> energy_snapshots() const override
  {
    return {{cost.name, revision, cost.assumptions, dynamic_energy}};
  }
};

double xor_fold(unsigned input_bits, unsigned output_bits)
{
  double energy = 0;
  for (unsigned bit = 0; bit < output_bits && bit < input_bits; ++bit) {
    const auto terms = 1 + (input_bits - bit - 1) / output_bits;
    if (terms > 1)
      energy += hcm::reduction(terms, [](double load) { return hcm::xor2(load); }, hcm::OUTCAP).e;
  }
  return energy;
}

double tag_hash_energy(litebtb_hash algorithm, bool two_level, unsigned width, unsigned input_bits)
{
  if (width == 0)
    return 0;
  switch (algorithm) {
  case litebtb_hash::identity:
  case litebtb_hash::rabin:
    return 0;
  case litebtb_hash::half_xor: {
    const auto unchanged = std::min(input_bits, width / 2);
    return xor_fold(input_bits - unchanged, width - width / 2);
  }
  case litebtb_hash::full_xor:
    return xor_fold(input_bits, width);
  case litebtb_hash::geometric: {
    if (width >= input_bits || width == 1)
      return 0;
    const auto upper_bits = input_bits - width;
    const auto shift_stages = storage_bits(upper_bits + 1);
    // reverse_bits() depends on the position of the highest set bit.
    // Model a fixed reversal, leading-zero detection and a barrel shifter.
    const auto reverse = select_way(upper_bits) + static_cast<double>(shift_stages) * mux_energy(2, upper_bits);
    unsigned used = 0;
    for (unsigned bit = 1; bit < width && used < upper_bits; ++bit) {
      const double power = std::pow(1.1, bit);
      const auto group = static_cast<unsigned>(two_level ? std::round(power) : std::round((power - 1) / 0.1) - 1);
      used += std::min(group, upper_bits - used);
    }
    return reverse + used * xor_bit;
  }
  case litebtb_hash::pearson:
    // Constant permutations modeled as 256 x 8 ROM tables with SRAM read cost.
    return ((width + 7) / 8) * 8 * (8 * xor_bit + hcm::sram<256, 8>::EREAD);
  case litebtb_hash::sha3: {
    // One Keccak-f[1600] permutation. Rotation and lane permutation are wires.
    // A constant iota XOR is represented by inverters in the high-level model.
    constexpr auto iota_inverted_bits = 86;
    return 24 * 64 * (75 * xor_bit + 25 * (hcm::AND<2>.e + hcm::INV.e)) + iota_inverted_bits * hcm::INV.e;
  }
  case litebtb_hash::zobrist:
    // Immutable SplitMix outputs are constants. Half their selected bits
    // participate on average in the XOR network for each retained output bit.
    return width * std::max(0.0, static_cast<double>(input_bits) / 2 - 1) * xor_bit;
  }
  return 0;
}

struct way_cost {
  access_cost tag;
  access_cost payload;
  access_cost decode;
  access_cost owner;
};

struct table_cost {
  std::vector<way_cost> ways;
  double read{};
  double promote{};
  double choose{};
  double payload_select{};
  double invalidate{};
  double hash{};
};

table_cost characterize_table(std::size_t sets, const std::vector<litebtb_energy_geometry::way>& ways,
                              const litebtb_energy_geometry& geometry)
{
  table_cost table;
  const auto rank_width = storage_bits(ways.size());
  const auto ranks = sram_row(sets, ways.size() * rank_width);
  const double compare_rank = rank_width == 0 ? 0 : hcm::unsigned_greater_than(rank_width, hcm::OUTCAP).e;
  table.promote = ranks.read + ranks.write + rank_update(ways.size());
  table.choose = static_cast<double>(ways.size() - 1) * (compare_rank + mux_energy(2, rank_width) + hcm::AND<2>.e);
  table.invalidate = sram_row(sets, 1).write;
  table.read = select_way(ways.size());
  const bool tokens = geometry.utb_entries != 0;
  std::set<unsigned> hash_widths;
  for (const auto& way : ways) {
    const auto key_width = way.utb_pointer ? storage_bits(geometry.utb_entries) : way.tag;
    const auto target_bits = std::min(64U, way.target + 2);
    const auto saved_pc = way.reconstruct_from_lookup_pc ? 0 : 64 - target_bits;
    // Three precise-type bits encode both the type used by prediction and
    // the type used by decode when UTB repair is enabled.
    const auto metadata = tokens ? 3 + generation_bits * (way.utb_pointer ? 2 : 1) : 2;
    const auto payload_bits = target_bits + saved_pc + metadata;
    const auto compare = match_energy(key_width) + hcm::AND<2>.e;
    const way_cost cost{sram_row(sets, key_width + 1), sram_row(sets, payload_bits),
                        sram_row(sets, tokens ? 1 + 3 + generation_bits : 0),
                        sram_row(sets, way.utb_pointer ? generation_bits : 0)};
    table.ways.push_back(cost);
    table.read += cost.tag.read + compare;
    if (way.hashed && !way.utb_pointer)
      hash_widths.insert(way.tag);
  }
  // Join the low target bits to the upper saved-PC or lookup-PC bits
  // before the mux. Generation metadata stays in the decode path.
  table.payload_select = mux_energy(ways.size(), 64 + (tokens ? 3 : 2)) + hcm::AND<2>.e;
  for (const auto width : hash_widths)
    table.hash += tag_hash_energy(geometry.hash, geometry.two_level, width, 62 - static_cast<unsigned>(storage_bits(sets)));
  return table;
}

struct utb_cost {
  double lookup{}, promote{}, replace{}, owner{}, reference{}, pointer_compare{};
};

utb_cost characterize_utb(const litebtb_energy_geometry& geometry)
{
  if (geometry.utb_entries == 0)
    return {};
  const auto ways = geometry.utb_ways;
  const auto sets = geometry.utb_entries / ways;
  const auto rank_bits = storage_bits(ways);
  const auto ref_bits = storage_bits(geometry.sets * geometry.ways.size() + 1);
  const auto tags = sram_row(sets, geometry.utb_tag_bits + 1);
  const auto ranks = sram_row(sets, ways * rank_bits);
  const auto generation = sram_row(sets, generation_bits);
  const auto refs = sram_row(sets, ref_bits);
  const auto key_width = geometry.utb_tag_bits + storage_bits(sets);
  const auto hash = tag_hash_energy(geometry.hash, geometry.two_level, static_cast<unsigned>(key_width),
                                    62 - static_cast<unsigned>(storage_bits(geometry.sets)));
  const auto double_ways = static_cast<double>(ways);
  const auto read = double_ways * (tags.read + match_energy(geometry.utb_tag_bits) + hcm::AND<2>.e)
      + select_way(ways) + mux_energy(ways, rank_bits) + hash;
  const auto promote = ranks.read + ranks.write + rank_update(ways);
  const auto eviction = (geometry.utb_reference_count ? double_ways * refs.read + (double_ways - 1)
      * (match_energy(ref_bits) + hcm::unsigned_greater_than(ref_bits, hcm::OUTCAP).e + mux_energy(2, ref_bits)) : 0)
      + (rank_bits == 0 ? 0 : (double_ways - 1) * hcm::unsigned_greater_than(rank_bits, hcm::OUTCAP).e);
  return {read, promote, eviction + promote + tags.write + generation.write + refs.write,
          generation.read + hcm::EQUAL<generation_bits>.e,
          refs.read + refs.write + increment_energy(ref_bits),
          match_energy(storage_bits(geometry.utb_entries)) + hcm::AND<2>.e};
}

struct litebtb_names {
  std::string_view l2, l1, utb, indirect, ras;
};

#define LITEBTB_NAMES(name) {name " target table", name " L1 target table", name " UTB", name " ITTAGE", name " return-address stack"}
constexpr litebtb_names single_level[] = {
    LITEBTB_NAMES("xbtbx"), LITEBTB_NAMES("hash_xbtbx"), LITEBTB_NAMES("fullxor_hash_xbtbx"), LITEBTB_NAMES("geometric_xbtbx"),
    LITEBTB_NAMES("pearson_xbtbx"), LITEBTB_NAMES("rabin_xbtbx"), LITEBTB_NAMES("sha3_xbtbx"), LITEBTB_NAMES("zobrist_xbtbx")};
constexpr litebtb_names two_level[] = {
    LITEBTB_NAMES("two_level_xbtbx"), LITEBTB_NAMES("two_level_hashed_xbtbx"),
    LITEBTB_NAMES("two_level_full_hashed_xbtbx"), LITEBTB_NAMES("two_level_geometric_hashed_xbtbx")};
#undef LITEBTB_NAMES

class litebtb_model final : public predictor_energy_model
{
  const litebtb_names names;
  const table_cost l2;
  const table_cost l1;
  const utb_cost utb;
  const access_cost ras;
  const double ras_pointer;
  const double pointer_recheck;
  const bool has_utb;
  const bool reference_count;
  double l1_energy{}, l2_energy{}, utb_energy{}, ittage_energy{}, ras_energy{};

  void record_table(double& energy, const table_cost& table, predictor_event event, std::size_t arg)
  {
    switch (event) {
    case predictor_event::l1_read:
    case predictor_event::l2_read: energy += table.read + table.hash; break;
    case predictor_event::l1_select:
    case predictor_event::l2_select: {
      const auto& way = table.ways.at(arg);
      energy += way.payload.read + table.payload_select;
      break;
    }
    case predictor_event::l1_promote:
    case predictor_event::l2_promote: energy += table.promote; break;
    case predictor_event::l1_fill_select:
    case predictor_event::l2_fill_select: energy += table.choose; break;
    case predictor_event::l1_write:
    case predictor_event::l2_write: {
      const auto& way = table.ways.at(arg);
      energy += way.tag.write + way.payload.write + table.promote;
      if (has_utb)
        energy += hcm::INC<generation_bits>.e + generation_bits * flop_bit;
      break;
    }
    case predictor_event::decode_l1:
    case predictor_event::decode_l2:
      energy += table.ways.at(arg).decode.read + hcm::EQUAL<generation_bits>.e + hcm::EQUAL<3>.e + hcm::AND<3>.e;
      break;
    case predictor_event::invalidate_l1:
    case predictor_event::invalidate_l2: energy += table.invalidate; break;
    default: break;
    }
  }

public:
  explicit litebtb_model(const litebtb_energy_geometry& geometry)
      : names((geometry.two_level && static_cast<unsigned>(geometry.hash) < std::size(two_level) ? two_level : single_level)
              [static_cast<unsigned>(geometry.hash)]),
        l2(characterize_table(geometry.sets, geometry.ways, geometry)),
        l1(geometry.two_level ? characterize_table(geometry.l1_sets,
                          std::vector<litebtb_energy_geometry::way>(geometry.l1_ways,
                              {62 - static_cast<unsigned>(storage_bits(geometry.l1_sets)), 64, false, false, false}), geometry) : table_cost{}),
        utb(characterize_utb(geometry)), ras(sram_row(geometry.ras_entries, 64)),
        ras_pointer(increment_energy(storage_bits(geometry.ras_entries)) + static_cast<double>(storage_bits(geometry.ras_entries + 1)) * flop_bit),
        pointer_recheck(utb.pointer_compare * static_cast<double>(std::count_if(geometry.ways.begin(), geometry.ways.end(),
                                                                              [](const auto& way) { return way.utb_pointer; }))),
        has_utb(geometry.utb_entries != 0), reference_count(geometry.utb_reference_count)
  {
  }

  void record(predictor_event event, std::size_t argument) override
  {
    switch (event) {
    case predictor_event::l1_read: case predictor_event::l1_select: case predictor_event::l1_promote: case predictor_event::l1_write:
    case predictor_event::l1_fill_select: case predictor_event::decode_l1: case predictor_event::invalidate_l1:
      record_table(l1_energy, l1, event, argument);
      break;
    case predictor_event::l2_read: case predictor_event::l2_select: case predictor_event::l2_promote: case predictor_event::l2_write:
    case predictor_event::l2_fill_select: case predictor_event::decode_l2: case predictor_event::invalidate_l2:
      record_table(l2_energy, l2, event, argument);
      break;
    case predictor_event::l2_recheck: l2_energy += pointer_recheck; break;
    case predictor_event::target_width: l2_energy += 62 * xor_bit + hcm::priority_encoder<62>.e; break;
    case predictor_event::utb_read: utb_energy += utb.lookup; break;
    case predictor_event::utb_promote: utb_energy += utb.promote; break;
    case predictor_event::utb_replace: utb_energy += utb.replace + hcm::INC<generation_bits>.e + generation_bits * flop_bit; break;
    case predictor_event::utb_owner:
      utb_energy += utb.owner;
      l2_energy += l2.ways.at(argument).owner.read;
      break;
    case predictor_event::utb_reference: utb_energy += utb.reference; break;
    case predictor_event::ras_empty: ras_energy += hcm::AND<2>.e; break;
    default: break;
    }
  }

  void record(frontend_event event, std::size_t argument) override
  {
    switch (event) {
    case frontend_event::ras_push: ras_energy += ras.write + ras_pointer; break;
    case frontend_event::ras_pop: ras_energy += ras_pointer; break;
    case frontend_event::ras_return: ras_energy += ras.read + hcm::INC<62>.e + hcm::AND<2>.e; break;
    default: ittage_energy += harcom::ittage_energy(event, argument); break;
    }
  }

  std::vector<energy_model_snapshot> energy_snapshots() const override
  {
    std::vector<energy_model_snapshot> result{
        {names.l2, revision, names.l2 == single_level[static_cast<unsigned>(litebtb_hash::rabin)].l2
             ? "Parallel tag ways, gated target payload and LRU logic; Rabin truncates tags and reconstructs targets from the lookup PC"
             : "Parallel tag ways, gated target payload, hash and LRU logic; inline compressed targets retain upper PC bits",
         l2_energy},
        {names.indirect, revision, "Parallel base and tagged SRAMs, resettable useful bits and shift-register history", ittage_energy},
        {names.ras, revision, "64 entries, depth and stack pointer, constant four-byte return displacement", ras_energy}};
    if (!l1.ways.empty())
      result.push_back({names.l1, revision, "64 sets, eight ways, full tags and targets; prediction probes L2 only on an L1 miss", l1_energy});
    if (has_utb)
      result.push_back({names.utb, revision, reference_count ? "Banked upper tags, generation tokens, reference counters and LRU tie-break"
                                                           : "Banked upper tags and generation tokens, LRU with reference bookkeeping", utb_energy});
    return result;
  }
};

std::unique_ptr<predictor_energy_model> make_direction_model(predictor_kind kind)
{
  switch (kind) {
  case predictor_kind::bimodal: return std::make_unique<direction_model>(bimodal_cost());
  case predictor_kind::perceptron: return std::make_unique<direction_model>(perceptron_cost());
  case predictor_kind::hashed_perceptron: return std::make_unique<direction_model>(hashed_perceptron_cost());
  }
  throw std::invalid_argument("Unknown HARCOM direction predictor");
}

std::unique_ptr<predictor_energy_model> make_litebtb_model(const litebtb_energy_geometry& geometry)
{
  return std::make_unique<litebtb_model>(geometry);
}
} // namespace

void configure_harcom_predictors(O3_CPU& cpu)
{
  cpu.make_direction_energy = make_direction_model;
  cpu.make_litebtb_energy = make_litebtb_model;
}
} // namespace champsim
