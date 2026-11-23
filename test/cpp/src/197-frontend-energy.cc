#include <map>
#include <catch.hpp>

#include "../../../btb/basic_btb/basic_btb.h"
#include "../../../prefetcher/fdip/fdip.h"
#include "cache.h"
#include "defaults.hpp"
#include "frontend_energy.h"
#include "ooo_cpu.h"

namespace
{
using champsim::frontend_event;

struct recording_model : champsim::frontend_energy_model {
  std::map<frontend_event, std::size_t> events;
  void record(frontend_event event, std::size_t argument) override { events[event] += argument; }
  [[nodiscard]] std::vector<champsim::energy_model_snapshot> energy_snapshots() const override { return {}; }
  [[nodiscard]] std::vector<std::string> coverage_gaps() const override { return {}; }
};

struct observed_btb : basic_btb {
  observed_btb(O3_CPU* cpu, recording_model& model) : basic_btb(cpu) { set_energy_model(&model); }
};
} // namespace

TEST_CASE("Frontend accounting observes FTQ operations at the prediction queue")
{
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1i}.prefetcher<fdip>()};
  O3_CPU cpu{champsim::core_builder{}.l1i(&cache).fdip_ftq_size(1)};
  recording_model model;
  cpu.frontend_energy = &model;

  cpu.redirect_fdip(champsim::address{0x1000});
  CHECK_FALSE(cpu.consume_prediction(champsim::address{0x1000}));
  cpu.advance_fdip();
  cpu.advance_fdip(); // The full queue prevents a second block write.
  CHECK_FALSE(cpu.consume_prediction(champsim::address{0x2000}));
  for (uint64_t ip = 0x1000; ip < 0x1020; ip += 4)
    REQUIRE(cpu.consume_prediction(champsim::address{ip}));

  CHECK(model.events[frontend_event::ftq_redirect] == 1);
  CHECK(model.events[frontend_event::fdip_slot] == 8);
  CHECK(model.events[frontend_event::ftq_write] == 8);
  CHECK(model.events[frontend_event::ftq_push] == 1);
  CHECK(model.events[frontend_event::ftq_read] == 9);
  CHECK(model.events[frontend_event::ftq_consume] == 8);
  CHECK(model.events[frontend_event::ftq_pop] == 1);
}

TEST_CASE("ITTAGE accounting observes lookup, training and history as separate operations")
{
  ittage predictor;
  recording_model model;
  predictor.set_energy_model(&model);
  const champsim::address ip{0x2000}, target{0x9000};
  (void)predictor.prediction(ip);
  predictor.update(ip, target, true, BRANCH_INDIRECT_CALL);
  predictor.update(ip, target, true, BRANCH_CONDITIONAL);
  CHECK(model.events[frontend_event::ittage_lookup] == 2);
  CHECK(model.events[frontend_event::ittage_train] == 1);
  CHECK(model.events[frontend_event::ittage_tagged_write] == 1);
  CHECK(model.events[frontend_event::ittage_base_write] == 1);
  CHECK(model.events[frontend_event::ittage_history_mix] == 1);
  CHECK(model.events[frontend_event::ittage_history_bit] == 11);
}

TEST_CASE("Speculative returns use the sizes at the last FDIP checkpoint")
{
  CACHE cache{champsim::cache_builder{champsim::defaults::default_l1i}.prefetcher<fdip>()};
  O3_CPU cpu{champsim::core_builder{}.l1i(&cache)};
  recording_model model;
  observed_btb btb{&cpu, model};
  const champsim::address call{0x2100}, entry{0x3000}, ret{0x3010};

  btb.update_btb(call, entry, true, BRANCH_DIRECT_CALL);
  btb.update_btb(ret, call + 4, true, BRANCH_RETURN);
  btb.update_btb(call, entry, true, BRANCH_DIRECT_CALL);
  (void)btb.btb_prediction(entry); // Take a checkpoint with one live stack entry.
  cpu.fetch_target_queue.emplace_back();
  btb.update_btb(ret, call + 8, true, BRANCH_RETURN);
  btb.update_btb(call, entry, true, BRANCH_DIRECT_CALL);

  CHECK(btb.btb_prediction(ret).first == call + 4);
  cpu.fetch_target_queue.clear();
  ++cpu.sim_stats.fdip_fetch_blocks;
  CHECK(btb.btb_prediction(ret).first == call + 8);
  CHECK(model.events[frontend_event::ras_checkpoint] == 2);
  CHECK(model.events[frontend_event::ras_speculative_return] == 2 * return_stack::call_size_index(call));
  CHECK(model.events[frontend_event::ras_calibration_write] == 2 * return_stack::call_size_index(call));
}
