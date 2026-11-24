// The adapter reads geometry and activity from the simulator components.
#undef CHAMPSIM_MODULE
#include "../sram_cost.h"
#include "../../prefetcher/fdip/fdip.h"
#include "../../replacement/drrip/drrip.h"
#include "environment.h"
#include "memory_energy.h"
#include "storage.h"

#include <algorithm>
#include <stdexcept>

namespace champsim
{
namespace
{
constexpr std::string_view revision = "5ceea8358ddbab94e5621e2536530b860cc1e5b7";

using harcom::match_energy;
using harcom::sram_row;

struct meter {
  struct term {
    const uint64_t* count;
    double femtojoules;
  };
  std::string instance;
  std::string_view model;
  std::string_view assumptions;
  chrono::picoseconds clock_period;
  std::vector<term> terms;

  void add(const uint64_t& count, double femtojoules) { terms.push_back({&count, femtojoules}); }
  void add(const memory_activity& activity, memory_event event, double femtojoules) { add(activity[event], femtojoules); }
};

class harcom_memory_model final : public memory_energy_model
{
  std::vector<meter> meters;
  std::vector<std::string> gaps{
      "cache and translation routing, wire loads and global bank wiring",
      "translation storage outside modeled TLBs and page-walk caches"};

  meter& add_meter(std::string instance, std::string_view model, std::string_view assumptions, chrono::picoseconds period)
  {
    meters.push_back({std::move(instance), model, assumptions, period, {}});
    return meters.back();
  }

  void add_queues(const std::string& name, chrono::picoseconds period, const std::vector<channel*>& channels, std::size_t write_payload)
  {
    // Address, virtual address, PC, ASID, metadata, type and request flags.
    constexpr auto request_bits = 64 * 3 + 16 + 32 + 4 + 2;
    if (channels.empty())
      return;
    auto& queues = add_meter(name + " request queues", "HARCOM request FIFOs",
                             "Consumer-owned RQ/WQ/PQ SRAMs charge accepted writes and consumed reads. The WQ carries the data payload.", period);
    for (auto* input : channels) {
      if (!input->energy_activity)
        input->energy_activity.emplace();
      const std::array depths{input->rq_size(), input->wq_size(), input->pq_size()};
      for (std::size_t index = 0; index < depths.size(); ++index) {
        if (depths[index] == std::numeric_limits<std::size_t>::max()) {
          gaps.push_back(name + " unbounded input queue geometry");
          continue;
        }
        const auto payload = index == static_cast<std::size_t>(request_queue::write) ? write_payload : 0;
        const auto word = sram_row(depths[index], request_bits + payload);
        const auto& activity = (*input->energy_activity)[index];
        queues.add(activity.reads, word.read + hcm::flipflops<16>::write_energy_fJ);
        queues.add(activity.writes, word.write + hcm::flipflops<16>::write_energy_fJ);
      }
    }
  }

  void add_mshr(const std::string& name, chrono::picoseconds period, const memory_activity& activity, std::size_t rows, std::size_t compare_bits)
  {
    auto& model = add_meter(name + " miss tracking", "HARCOM MSHR and fill search",
                            "Parallel valid-entry address matching, SRAM rows for live MSHRs, and fill-queue address comparisons.", period);
    constexpr auto mshr_bits = 64 * 3 + 32 + 16 + 8;
    const auto word = sram_row(rows, mshr_bits);
    model.add(activity, memory_event::mshr_compare, match_energy(compare_bits));
    model.add(activity, memory_event::fill_compare, match_energy(compare_bits));
    model.add(activity, memory_event::mshr_read, word.read);
    model.add(activity, memory_event::mshr_write, word.write);
    model.add(activity, memory_event::mshr_release, hcm::flipflops<1>::write_energy_fJ);
  }

  void add_replacement(const CACHE& cache, const memory_activity& activity, replacement_energy_policy policy)
  {
    if (policy == replacement_energy_policy::unmanaged) {
      gaps.push_back(cache.NAME + " unsupported replacement policy");
      return;
    }
    const bool lru = policy == replacement_energy_policy::lru;
    auto& state = add_meter(cache.NAME + " replacement state", "HARCOM replacement metadata",
                            lru ? "one per-set LRU rank row" : "one per-set row of two-bit re-reference prediction values", cache.clock_period);
    const auto word = sram_row(cache.NUM_SET, cache.NUM_WAY * (lru ? storage_bits(cache.NUM_WAY) : 2));
    state.add(activity, memory_event::replacement_victim, word.read + (policy == replacement_energy_policy::srrip ? word.write : 0));
    state.add(activity, memory_event::replacement_hit, word.read + word.write);
    state.add(activity, memory_event::replacement_fill, word.read + word.write);
    if (!lru || cache.ubs)
      state.add(activity, memory_event::replacement_writeback_hit, word.read + word.write);
    if (policy == replacement_energy_policy::drrip) {
      state.add(activity, memory_event::drrip_age_row, word.write + static_cast<double>(cache.NUM_WAY) * hcm::ADD<2>.e);
      auto& selector = add_meter(cache.NAME + " DRRIP policy selection", "HARCOM DRRIP control",
                                 "Sample-set classifier, per-core saturating policy selector, and periodic BRRIP insertion counter.", cache.clock_period);
      const auto set_bits = storage_bits(msl::get_sample_rate(cache.NUM_SET));
      selector.add(activity, memory_event::drrip_classify, hcm::subtract(set_bits, hcm::OUTCAP).e + 2 * match_energy(set_bits));
      constexpr auto selector_width = drrip::PSEL_WIDTH;
      const auto per_core = sram_row(NUM_CPUS, selector_width);
      selector.add(activity, memory_event::drrip_selector_read, per_core.read + hcm::GTE<selector_width>.e);
      selector.add(activity, memory_event::drrip_selector_update,
                   per_core.read + per_core.write + hcm::INC<selector_width>.e + hcm::EQUAL<selector_width>.e);
      constexpr auto insertion_width = champsim::lg2(drrip::BRRIP_MAX);
      selector.add(activity, memory_event::drrip_brrip_tick,
                   hcm::INC<insertion_width>.e + hcm::EQUAL<insertion_width>.e + hcm::flipflops<insertion_width>::write_energy_fJ);
    }
  }

  void add_refills(const std::string& name, chrono::picoseconds period, const memory_activity& activity, std::size_t payload_bits)
  {
    auto& stage = add_meter(name + " refill staging and response ports", "HARCOM refill and response registers",
                            "Register banks hold ready entries and drive one write per receiving response port. Wire and routing costs remain open.",
                            period);
    constexpr auto slice_bits = 32;
    constexpr auto word_write = hcm::flipflops<slice_bits>::write_energy_fJ;
    constexpr auto metadata_bits = 256;
    constexpr auto response_bits = 128 + 32;
    const auto payload_slices = static_cast<double>((payload_bits + slice_bits - 1) / slice_bits);
    const auto staged_read = (metadata_bits + payload_slices * slice_bits) * hcm::INV.e;
    const auto staged_write = (metadata_bits / slice_bits + payload_slices) * word_write;
    stage.add(activity, memory_event::fill_stage_read, staged_read);
    stage.add(activity, memory_event::fill_stage_write, staged_write);
    stage.add(activity, memory_event::fill_stage_merge, metadata_bits * hcm::INV.e + metadata_bits / slice_bits * word_write);
    stage.add(activity, memory_event::fill_stage_release, hcm::flipflops<1>::write_energy_fJ);
    stage.add(activity, memory_event::response_delivery, (response_bits / slice_bits + payload_slices) * word_write);
  }

  void add_ubs(CACHE& cache, const memory_activity& activity, std::size_t tag_width, std::size_t line_bits)
  {
    constexpr std::size_t offset_width = 6;
    const auto tags = sram_row(cache.NUM_SET, tag_width + offset_width + 3);
    auto& fragment_tags = add_meter(cache.NAME + " UBS fragment tags", "HARCOM UBS fragment tags and masks",
                                    "Parallel tag ways store offsets and use fixed way sizes. Matching fragment masks are merged.", cache.clock_period);
    fragment_tags.add(activity, memory_event::tag_lookup, static_cast<double>(cache.NUM_WAY) * (tags.read + match_energy(tag_width)));
    fragment_tags.add(activity, memory_event::ubs_candidate_metadata, tags.read);
    fragment_tags.add(activity, memory_event::tag_write, tags.write);
    constexpr auto mask_mux = hcm::MUX<2, 64>;
    fragment_tags.add(activity, memory_event::ubs_fragment_mask, 6 * (mask_mux[0].e + mask_mux[1].e) + 64 * hcm::OR<2>.e);

    cache.ubs_fragment_accesses.resize(cache.UBS_WAY_SIZES.size());
    auto& fragment_data = add_meter(cache.NAME + " UBS fragment payloads", "HARCOM UBS variable-width arrays",
                                    "one SRAM per physical way, sized by its fragment capacity", cache.clock_period);
    for (std::size_t way = 0; way < cache.UBS_WAY_SIZES.size(); ++way) {
      const auto word = sram_row(cache.NUM_SET, 8 * cache.UBS_WAY_SIZES[way]);
      const auto& access = cache.ubs_fragment_accesses[way];
      fragment_data.add(access.reads, word.read);
      fragment_data.add(access.writes, word.write);
    }

    const auto predictor_meta = sram_row(cache.NUM_SET, tag_width + 3 + 64 + 32);
    const auto predictor_data = sram_row(cache.NUM_SET, line_bits);
    auto& predictor = add_meter(cache.NAME + " UBS predictor", "HARCOM UBS full-line predictor",
                                "one full line per set with tag, status, useful-byte mask and prefetch metadata", cache.clock_period);
    predictor.add(activity, memory_event::ubs_predictor_lookup, predictor_meta.read + match_energy(tag_width));
    predictor.add(activity, memory_event::ubs_predictor_read, predictor_data.read);
    predictor.add(activity, memory_event::ubs_predictor_write, predictor_data.write);
    predictor.add(activity, memory_event::ubs_predictor_metadata_write, predictor_meta.write + 64 * hcm::OR<2>.e);
  }

  void add_fdip(const CACHE& cache, const memory_activity& activity)
  {
    auto& state = add_meter(cache.NAME + " FDIP target and recent queues", "HARCOM FDIP prefetch state",
                            "The register target FIFO and recent-entry comparisons belong to the FDIP module.", cache.clock_period);
    constexpr auto targets = fdip::MAX_QUEUED_TARGETS;
    constexpr auto recent = fdip::MAX_RECENT_PREFETCHES;
    constexpr auto head = hcm::MUX<targets, 64>;
    const auto block_bits = 64 - to_underlying(cache.OFFSET_BITS);
    state.add(activity, memory_event::fdip_target_compare, match_energy(64));
    state.add(activity, memory_event::fdip_target_read, head[0].e + head[1].e);
    state.add(activity, memory_event::fdip_target_write,
              hcm::decoder<targets>.e + hcm::flipflops<64>::write_energy_fJ + hcm::flipflops<16>::write_energy_fJ);
    state.add(activity, memory_event::fdip_target_drop, hcm::flipflops<16>::write_energy_fJ);
    state.add(activity, memory_event::fdip_target_reset, hcm::flipflops<23>::write_energy_fJ);
    state.add(activity, memory_event::fdip_recent_compare, match_energy(block_bits));
    state.add(activity, memory_event::fdip_recent_write, hcm::decoder<recent>.e + hcm::flipflops<64>::write_energy_fJ);
    state.add(activity, memory_event::fdip_recent_drop, hcm::flipflops<1>::write_energy_fJ);
    constexpr auto shift = hcm::MUX<2, 64>;
    state.add(activity, memory_event::fdip_request_mask, 6 * (shift[0].e + shift[1].e) + hcm::GTE<6>.e);
  }

  void add_fdip_directory(const CACHE& cache, const memory_activity& activity)
  {
    auto& directory = add_meter(cache.NAME + " FDIP address directories", "HARCOM mirrored address comparisons",
                                "One mirrored resident key per slot and one key per live pending entry. Each searched group compares in parallel.",
                                cache.clock_period);
    const auto block_bits = 64 - to_underlying(cache.OFFSET_BITS);
    const auto compare = match_energy(block_bits) + hcm::AND<2>.e + hcm::OR<2>.e;
    directory.add(activity, memory_event::fdip_resident_compare, compare);
    directory.add(activity, memory_event::fdip_resident_key_write,
                  cache.ubs ? hcm::flipflops<72>::write_energy_fJ : hcm::flipflops<64>::write_energy_fJ);
    constexpr auto shift = hcm::MUX<2, 64>;
    directory.add(activity, memory_event::fdip_resident_mask, 6 * (shift[0].e + shift[1].e) + 64 * hcm::OR<2>.e);
    directory.add(activity, memory_event::fdip_pending_compare, compare);

    constexpr auto key_write = hcm::flipflops<64>::write_energy_fJ;
    for (auto event : {memory_event::fdip_pipeline_key_write, memory_event::local_prefetch_write, memory_event::mshr_write,
                       memory_event::fill_stage_write, memory_event::fill_stage_merge}) {
      directory.add(activity, event, key_write);
    }
    for (const auto* input : cache.upper_levels) {
      for (const auto& queue : *input->energy_activity)
        directory.add(queue.writes, key_write);
    }
  }

  void add_cache(CACHE& cache, cache_energy_configuration config)
  {
    auto& activity = cache.memory_activity.emplace();
    const auto offset_bits = to_underlying(cache.OFFSET_BITS);
    const auto index_bits = storage_bits(cache.NUM_SET);
    if (cache.NUM_SET == 0 || cache.NUM_WAY == 0 || offset_bits >= 64 || index_bits >= 64 - offset_bits
        || (!config.translation && offset_bits > 12)) {
      gaps.push_back(cache.NAME + " unsupported tag or payload geometry");
      return;
    }
    const auto payload = config.translation ? 64 - offset_bits : uint64_t{8} << offset_bits;
    add_queues(cache.NAME, cache.clock_period, cache.upper_levels, payload);
    add_mshr(cache.NAME, cache.clock_period, activity, cache.MSHR_SIZE, 64 - offset_bits);
    add_refills(cache.NAME, cache.clock_period, activity, payload);
    if (cache.PQ_SIZE != 0) {
      auto& local = add_meter(cache.NAME + " local prefetch queue", "HARCOM prefetch FIFO",
                              "separate local request SRAM, charged on admission and consumption", cache.clock_period);
      const auto word = sram_row(cache.PQ_SIZE, 64 * 3 + 16 + 32 + 4 + 2);
      local.add(activity, memory_event::local_prefetch_read, word.read + hcm::flipflops<16>::write_energy_fJ);
      local.add(activity, memory_event::local_prefetch_write, word.write + hcm::flipflops<16>::write_energy_fJ);
    }
    if (config.has_unmodeled_prefetcher)
      gaps.push_back(cache.NAME + " prefetch algorithm tables and logic");
    if (config.fdip) {
      add_fdip(cache, activity);
      add_fdip_directory(cache, activity);
    } else if (cache.uses_prediction_flow()) {
      gaps.push_back(cache.NAME + " custom prediction-flow directory probes");
    }
    if (cache.ideal || cache.ideal_translation_vmem != nullptr || (cache.ubs && config.translation)) {
      gaps.push_back(cache.NAME + (cache.ubs && config.translation ? " unsupported UBS translation arrays" : " idealized tag and payload arrays"));
      if (cache.PREFETCH_BUFFER_SIZE != 0)
        gaps.push_back(cache.NAME + " prefetch buffer in unsupported cache mode");
      return;
    }

    const auto tag_width = 64 - offset_bits - index_bits;
    if (cache.ubs) {
      add_ubs(cache, activity, tag_width, payload);
      if (!cache.repl_module_pimpl->impl_supports_bounded_victims())
        config.replacement = replacement_energy_policy::lru; // UBS uses its built-in rank policy.
    } else {
      const auto tag = sram_row(cache.NUM_SET, tag_width + 3);
      auto& tags = add_meter(cache.NAME + " tags and status", "HARCOM parallel tag ways",
                             "All SRAM tag and status ways read in parallel. Writes select one way.", cache.clock_period);
      const auto ways = static_cast<double>(cache.NUM_WAY);
      tags.add(activity, memory_event::tag_lookup, ways * (tag.read + match_energy(tag_width)) + hcm::oring(cache.NUM_WAY, hcm::OUTCAP).e);
      tags.add(activity, memory_event::victim_metadata, ways * tag.read);
      tags.add(activity, memory_event::tag_write, tag.write);
      const auto data = sram_row(cache.NUM_SET, payload);
      auto& data_meter = add_meter(cache.NAME + " payload", "HARCOM selected-way array",
                                   config.translation ? "one physical-page-number payload per selected TLB row"
                                                      : "One cache-line SRAM payload per selected way. Fills and WRITE hits transfer full lines.", cache.clock_period);
      data_meter.add(activity, memory_event::data_read, data.read);
      data_meter.add(activity, memory_event::data_write, data.write);
    }
    add_replacement(cache, activity, config.replacement);

    if (cache.PREFETCH_BUFFER_SIZE != 0) {
      auto& buffer = add_meter(cache.NAME + " prefetch buffer", "HARCOM fully associative prefetch buffer",
                              "parallel valid-entry tag comparisons and a selected payload plus metadata row", cache.clock_period);
      const auto word = sram_row(cache.PREFETCH_BUFFER_SIZE, payload + 32);
      buffer.add(activity, memory_event::prefetch_compare, match_energy(64 - offset_bits));
      buffer.add(activity, memory_event::prefetch_read, word.read);
      buffer.add(activity, memory_event::prefetch_write, word.write + hcm::flipflops<16>::write_energy_fJ);
      buffer.add(activity, memory_event::prefetch_remove, hcm::flipflops<16>::write_energy_fJ);
    }
  }

  void add_walker(PageTableWalker& walker)
  {
    auto& activity = walker.memory_activity.emplace();
    add_queues(walker.NAME, walker.clock_period, walker.request_channels(), 64);
    add_refills(walker.NAME, walker.clock_period, activity, 64);
    auto& pending = add_meter(walker.NAME + " walk tracking", "HARCOM page-walk tracking",
                              "Parallel comparisons and per-entry registers scale with active walks.",
                              walker.clock_period);
    pending.add(activity, memory_event::mshr_compare, match_energy(64 - LOG2_BLOCK_SIZE));
    pending.add(activity, memory_event::mshr_read, 248 * hcm::INV.e);
    pending.add(activity, memory_event::mshr_write, hcm::flipflops<248>::write_energy_fJ);
    pending.add(activity, memory_event::mshr_release, hcm::flipflops<1>::write_energy_fJ);
    auto& control = add_meter(walker.NAME + " address generation", "HARCOM walker control",
                              "PTE address and response registers use the virtual memory mapping supplied by ChampSim.", walker.clock_period);
    control.add(activity, memory_event::walk_address, hcm::flipflops<64>::write_energy_fJ + hcm::SUB<3>.e);
    control.add(activity, memory_event::walk_complete, hcm::flipflops<64>::write_energy_fJ);

    for (std::size_t index = 0; index < walker.walk_cache_geometries.size(); ++index) {
      const auto& shape = walker.walk_cache_geometries[index];
      const auto index_bits = storage_bits(shape.sets);
      if (shape.offset_bits >= 64 || index_bits >= 64 - shape.offset_bits) {
        gaps.push_back(walker.NAME + " unsupported page-walk cache tag width");
        continue;
      }
      const auto tag_width = 64 - shape.offset_bits - index_bits;
      const auto tag = sram_row(shape.sets, tag_width + 1);
      const auto payload = sram_row(shape.sets, 64 - LOG2_PAGE_SIZE);
      const auto rank = sram_row(shape.sets, shape.ways * storage_bits(shape.ways));
      const auto& counts = walker.walk_cache_accesses[index];
      const auto ways = static_cast<double>(shape.ways);
      auto& cache = add_meter(fmt::format("{} PSCL{}", walker.NAME, shape.level), "HARCOM page-walk cache",
                              "parallel tags with selected next-table page number and LRU rank metadata", walker.clock_period);
      cache.add(counts.lookups, ways * (tag.read + match_energy(tag_width)));
      cache.add(counts.hits, payload.read + rank.read + rank.write);
      cache.add(counts.fills, ways * (tag.read + match_energy(tag_width)) + tag.write + payload.write + rank.read + rank.write);
    }
  }

public:
  harcom_memory_model(environment& env, std::initializer_list<cache_energy_configuration> configs)
  {
    const auto caches = env.cache_view();
    if (caches.size() != configs.size())
      throw std::invalid_argument("HARCOM memory configuration does not match the environment");
    auto config = configs.begin();
    for (CACHE& cache : caches)
      add_cache(cache, *config++);
    for (PageTableWalker& walker : env.ptw_view())
      add_walker(walker);
  }

  [[nodiscard]] std::vector<component_energy_stats> energy_snapshots() const override
  {
    std::vector<component_energy_stats> result;
    result.reserve(meters.size());
    for (const auto& meter : meters) {
      double energy = 0;
      for (const auto& term : meter.terms)
        energy += static_cast<double>(*term.count) * term.femtojoules;
      result.push_back({meter.instance, meter.model, revision, meter.assumptions, meter.clock_period, energy * 1e-15});
    }
    return result;
  }

  [[nodiscard]] std::vector<std::string> coverage_gaps() const override { return gaps; }
};
} // namespace

void configure_harcom_memory(environment& env, std::initializer_list<cache_energy_configuration> configs)
{
  env.memory_energy = std::make_unique<harcom_memory_model>(env, configs);
}
} // namespace champsim
