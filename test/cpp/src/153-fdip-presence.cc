#include <catch.hpp>

#include "cache.h"
#include "defaults.hpp"
#include "instruction.h"
#include "../../../prefetcher/fdip/fdip.h"
#include "../../../replacement/srrip/srrip.h"

TEST_CASE("FDIP uses identical target queues and prefetch packets with either cache", "[fdip-presence][icache-parity]")
{
  const bool uneven = GENERATE(false, true);
  champsim::channel lower;
  auto builder = champsim::cache_builder{champsim::defaults::default_l1i}
                     .sets(1).mshr_size(8).hit_latency(1).prefetch_buffer_size(0)
                     .reset_virtual_prefetch().lower_level(&lower);
  CACHE cache{uneven ? builder.set_ubs() : builder.reset_ubs()};
  cache.initialize();
  cache.warmup = false;
  fdip predictor{&cache};
  const champsim::address first{0x1020}, second{0x1030};
  predictor.prefetcher_prediction_operate(first);
  predictor.prefetcher_prediction_operate(second);
  predictor.prefetcher_prediction_operate(first);
  REQUIRE(predictor.queued_targets == std::deque<champsim::address>{first, second});
  predictor.prefetcher_cycle_operate();
  predictor.prefetcher_cycle_operate();
  REQUIRE(predictor.queued_targets.empty());
  REQUIRE(cache.sim_stats.pf_issued == 1);
  for (int cycle = 0; cycle < 10; ++cycle)
    cache._operate();
  REQUIRE(lower.PQ.size() == 1);
  REQUIRE(lower.PQ.front().address == first);
  REQUIRE(lower.PQ.front().requested_bytes == (uint64_t{0xf} << 32));
  // Completion permits another residency probe, even if the cache has since
  // discarded a line or UBS has retained only some of its instruction bytes.
  predictor.prefetcher_cache_fill(first, 0, 0, true, {}, 0);
  REQUIRE(predictor.recent_prefetches.empty());
  predictor.prefetcher_prediction_operate(second);
  predictor.prefetcher_cycle_operate();
  REQUIRE(cache.sim_stats.pf_issued == 2);
  predictor.prefetcher_cache_operate(second, {}, true, false, access_type::PREFETCH, 0);
  REQUIRE(predictor.recent_prefetches.empty());
  // The miss-triggered next-line path uses the same coverage metadata too.
  predictor.prefetcher_cache_operate(champsim::address{0x2000}, {}, false, false, access_type::LOAD, 7);
  for (int cycle = 0; cycle < 10; ++cycle)
    cache._operate();
  REQUIRE(lower.PQ.back().address == champsim::address{0x2040});
  REQUIRE(lower.PQ.back().requested_bytes == 0xf);
  REQUIRE(lower.PQ.back().pf_metadata == 7);
}

TEST_CASE("Matched sweep caches have identical timing and external resources", "[icache-parity]")
{
  const auto sets = GENERATE(8u, 16u, 32u, 64u, 128u, 256u);
  champsim::channel upper;
  auto builder = champsim::cache_builder{champsim::defaults::default_l1i}
                     .sets(sets).mshr_size(8).tag_bandwidth(champsim::bandwidth::maximum_type{2})
                     .fill_bandwidth(champsim::bandwidth::maximum_type{2}).upper_levels({&upper})
                     .prefetcher<fdip>().replacement<srrip>();
  CACHE conventional{champsim::cache_builder{builder}.ways(8)};
  CACHE uneven{champsim::cache_builder{builder}.set_ubs()};
  REQUIRE(conventional.NUM_WAY == 8);
  REQUIRE(uneven.NUM_WAY == 16);
  REQUIRE(conventional.NUM_SET == uneven.NUM_SET);
  REQUIRE(conventional.HIT_LATENCY == uneven.HIT_LATENCY);
  REQUIRE(conventional.FILL_LATENCY == uneven.FILL_LATENCY);
  REQUIRE(conventional.MSHR_SIZE == uneven.MSHR_SIZE);
  REQUIRE(conventional.PQ_SIZE == uneven.PQ_SIZE);
  REQUIRE(conventional.PREFETCH_BUFFER_SIZE == uneven.PREFETCH_BUFFER_SIZE);
  REQUIRE(conventional.PREFETCH_BUFFER_SIZE == 32);
  REQUIRE(conventional.MAX_TAG == uneven.MAX_TAG);
  REQUIRE(conventional.MAX_FILL == uneven.MAX_FILL);
  REQUIRE(conventional.pref_activate_mask == uneven.pref_activate_mask);
  REQUIRE(conventional.virtual_prefetch == uneven.virtual_prefetch);
  REQUIRE(conventional.uses_prediction_flow());
  REQUIRE(uneven.uses_prediction_flow());
}

TEST_CASE("FDIP skips resident virtual targets but issues missing targets", "[fdip-presence]")
{
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1i}.sets(2).ways(1).prefetch_buffer_size(0)};
  // Physical and virtual addresses intentionally differ, including the set index.
  auto& resident = cache.block[1];
  resident.valid = true;
  resident.address = champsim::address{0x2040};
  resident.v_address = champsim::address{0x1000};
  fdip predictor{&cache};
  predictor.prefetcher_prediction_operate(champsim::address{0x100c});
  predictor.prefetcher_prediction_operate(champsim::address{0x3000});
  predictor.prefetcher_cycle_operate();
  REQUIRE(predictor.queued_targets.empty());
  REQUIRE(cache.sim_stats.pf_requested == 1);
  REQUIRE(cache.sim_stats.pf_issued == 1);
  REQUIRE(predictor.recent_prefetches.size() == 1);
  REQUIRE(predictor.recent_prefetches.front() == champsim::block_number{champsim::address{0x3000}});
  REQUIRE(cache.is_instruction_present(champsim::address{0x100c}));
  REQUIRE_FALSE(cache.is_instruction_present(champsim::address{0x2040}));
}

TEST_CASE("FDIP checks UBS target bytes instead of treating a matching tag as a full hit", "[fdip-presence]")
{
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1i}.sets(1).set_ubs().prefetch_buffer_size(0)};
  auto& first = cache.block[0];
  first.valid = true;
  first.address = champsim::address{0x2000};
  first.v_address = champsim::address{0x1000};
  first.offset = 0;
  first.size = 4;
  REQUIRE(cache.is_instruction_present(champsim::address{0x1000}));
  REQUIRE_FALSE(cache.is_instruction_present(champsim::address{0x1004}));
  // Adjacent resident fragments together cover the target instruction.
  auto& second = cache.block[1];
  second = first;
  second.offset = 4;
  second.size = 4;
  REQUIRE(cache.is_instruction_present(champsim::address{0x1002}));
  fdip predictor{&cache};
  predictor.prefetcher_prediction_operate(champsim::address{0x1000});
  predictor.prefetcher_cycle_operate();
  REQUIRE(cache.sim_stats.pf_requested == 0);
  predictor.prefetcher_prediction_operate(champsim::address{0x1020});
  predictor.prefetcher_cycle_operate();
  REQUIRE(cache.sim_stats.pf_requested == 1);
}

TEST_CASE("FDIP UBS prefetches require all target instruction bytes", "[fdip-presence]")
{
  champsim::channel lower;
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1i}
                  .sets(1)
                  .mshr_size(8)
                  .set_ubs()
                  .prefetch_buffer_size(0)
                  .reset_virtual_prefetch()
                  .lower_level(&lower)
                  .hit_latency(1)};
  auto& resident = cache.block[0];
  resident.valid = true;
  resident.address = champsim::address{0x1000};
  resident.v_address = resident.address;
  // Byte zero and the first byte of the target are present, but its tail is missing.
  resident.offset = 0;
  resident.size = 33;
  fdip predictor{&cache};
  predictor.prefetcher_prediction_operate(champsim::address{0x1000});
  predictor.prefetcher_prediction_operate(champsim::address{0x1020});
  REQUIRE(predictor.queued_targets.size() == 2);
  predictor.prefetcher_cycle_operate();
  cache.initialize();
  cache.warmup = false;
  for (int cycle = 0; cycle < 20; ++cycle)
    cache._operate();
  REQUIRE(lower.PQ.size() == 1);
  REQUIRE(lower.PQ.front().address == champsim::address{0x1020});
  REQUIRE(lower.PQ.front().requested_bytes == (uint64_t{0xf} << 32));

  // While outstanding, another prediction is suppressed at line granularity.
  predictor.prefetcher_prediction_operate(champsim::address{0x1030});
  REQUIRE(predictor.queued_targets.empty());
  predictor.prefetcher_cache_fill(champsim::address{0x1000}, 0, 0, true, {}, 0);
  predictor.prefetcher_prediction_operate(champsim::address{0x1030});
  REQUIRE(predictor.queued_targets.size() == 1);
  predictor.prefetcher_cycle_operate();
  REQUIRE(cache.sim_stats.pf_issued == 2);
  predictor.prefetcher_cache_operate(champsim::address{0x1000}, {}, true, false, access_type::PREFETCH, 0);
  REQUIRE(predictor.recent_prefetches.empty());
}

TEST_CASE("FDIP prefetch misses cannot recursively walk past the prediction queue", "[fdip-presence][fdip-flow]")
{
  const bool uneven = GENERATE(false, true);
  champsim::channel lower;
  auto builder = champsim::cache_builder{champsim::defaults::default_l1i}
                     .sets(1).mshr_size(8).hit_latency(1).prefetch_buffer_size(0)
                     .prefetcher<fdip>().prefetch_activate(access_type::LOAD, access_type::PREFETCH)
                     .reset_virtual_prefetch().lower_level(&lower);
  CACHE cache{uneven ? builder.set_ubs() : builder.reset_ubs()};
  cache.initialize();
  cache.warmup = false;
  // The walker supplies precisely one fetch target, then stops (as at a full
  // FTQ or after a redirect). A miss must not create an independent stream.
  cache.prediction_operate(champsim::address{0x1000});
  for (int cycle = 0; cycle < 30; ++cycle)
    cache._operate();
  REQUIRE(cache.sim_stats.pf_issued == 1);
  REQUIRE(lower.PQ.size() == 1);
  REQUIRE(lower.PQ.front().address == champsim::address{0x1000});
}

TEST_CASE("A locally hitting FDIP prefetch cannot suppress requests after eviction", "[fdip-presence]")
{
  const bool uneven = GENERATE(false, true);
  champsim::channel lower;
  auto builder = champsim::cache_builder{champsim::defaults::default_l1i}
                     .sets(1).mshr_size(8).hit_latency(1).prefetch_buffer_size(0)
                     .reset_virtual_prefetch().lower_level(&lower);
  CACHE cache{uneven ? builder.set_ubs() : builder.reset_ubs()};
  cache.initialize();
  cache.warmup = false;
  fdip predictor{&cache};
  const champsim::address target{0x1020};
  predictor.prefetcher_prediction_operate(target);
  predictor.prefetcher_cycle_operate();
  REQUIRE(cache.is_instruction_request_pending(target));
  // A demand fill wins the race before this prefetch's tag lookup. Local
  // prefetch hits deliberately do not call prefetcher_cache_operate.
  auto& resident = cache.block[0];
  resident.valid = true;
  resident.address = target;
  resident.v_address = target;
  resident.offset = 0;
  resident.size = 64;
  for (int cycle = 0; cycle < 10; ++cycle)
    cache._operate();
  REQUIRE(lower.PQ.empty());
  REQUIRE_FALSE(cache.is_instruction_request_pending(target));
  REQUIRE(predictor.recent_prefetches.size() == 1);
  resident.valid = false;
  predictor.prefetcher_prediction_operate(target);
  REQUIRE(predictor.queued_targets == std::deque<champsim::address>{target});
  predictor.prefetcher_cycle_operate();
  REQUIRE(cache.sim_stats.pf_issued == 2);
}

TEST_CASE("FDIP pending probes use virtual tags through demand merges and refills", "[fdip-presence]")
{
  champsim::channel upper;
  champsim::channel lower;
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1i}
                  .upper_levels({&upper}).lower_level(&lower).hit_latency(1).fill_latency(10)};
  cache.initialize();
  cache.warmup = false;
  champsim::channel::request_type request;
  request.address = champsim::address{0x2040};
  request.v_address = champsim::address{0x1000};
  request.type = access_type::LOAD;
  upper.RQ.push_back(request);
  REQUIRE(cache.is_instruction_request_pending(champsim::address{0x1020}));
  REQUIRE_FALSE(cache.is_instruction_request_pending(request.address));
  for (int cycle = 0; cycle < 10; ++cycle)
    cache._operate();
  REQUIRE(upper.RQ.empty());
  REQUIRE(cache.MSHR.size() == 1);
  REQUIRE(cache.is_instruction_request_pending(champsim::address{0x1020}));
  REQUIRE_FALSE(cache.is_instruction_request_pending(request.address));
  lower.returned.emplace_back(request);
  cache._operate();
  REQUIRE(cache.MSHR.empty());
  REQUIRE(cache.inflight_fills.size() == 1);
  REQUIRE(cache.is_instruction_request_pending(champsim::address{0x1020}));
  for (int cycle = 0; cycle < 20; ++cycle)
    cache._operate();
  REQUIRE_FALSE(cache.is_instruction_request_pending(champsim::address{0x1020}));
}
