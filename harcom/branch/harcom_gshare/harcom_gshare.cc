#include "harcom_gshare.h"

#include <algorithm>

#include "../../leakage.h"

struct harcom_gshare::circuit {
  static constexpr auto index_bits = 14;
  static constexpr auto counter_bits = 2;

  static constexpr double leakage_watts()
  {
    using counters = hcm::sram<1 << index_bits, counter_bits>;
    constexpr auto select = hcm::MUX<2, counter_bits>;
    constexpr auto logic = hcm::XOR<2> * (3 * index_bits) || hcm::OR<2> * 3 || hcm::AND<2> * 2
        || hcm::INV || select[0] || select[1];
    return counters::leakage_mW() * 1e-3 + champsim::harcom::register_leakage_watts(index_bits)
        + champsim::harcom::logic_leakage_watts(logic);
  }

  hcm::region region;
  hcm::ram<hcm::val<counter_bits>, 1 << index_bits> table{"gshare counters"};
  hcm::reg<index_bits> history;
  hcm::u64 time = 0;

  hcm::val<index_bits> index(champsim::address ip)
  {
    auto pc = harcom_superuser::input<3 * index_bits>(ip.to<uint64_t>(), time);
    auto [high, middle, low] = hcm::split<index_bits, index_bits, index_bits>(pc.fo1());
    return low.fo1() ^ middle.fo1() ^ high.fo1() ^ history;
  }

  bool predict(champsim::address ip)
  {
    region.enter();
    auto counter = table.read(index(ip));
    // ChampSim gshare predicts taken for all nonzero counter states.
    auto direction = counter.fo1() != hcm::hard<0>{};
    time = harcom_superuser::ready_time(direction);
    harcom_superuser::next_cycle();
    return harcom_superuser::output(direction);
  }

  void update(champsim::address ip, bool taken)
  {
    region.enter();
    auto address = index(ip);
    address.fanout(hcm::hard<2>{});
    auto counter = table.read(address);
    harcom_superuser::next_cycle();

    auto outcome = harcom_superuser::input<1>(taken, time);
    outcome.fanout(hcm::hard<2>{});
    auto [high, low] = hcm::split<1, 1>(counter.fo1());
    high.fanout(hcm::hard<4>{});
    low.fanout(hcm::hard<3>{});
    auto inverted_low = ~low;
    inverted_low.fanout(hcm::hard<2>{});
    // The two-bit truth tables implement saturation at 0 and 3.
    auto increment = hcm::concat(high | low, high | inverted_low);
    auto decrement = hcm::concat(high & low, high & inverted_low);
    auto next = hcm::select(outcome, increment.fo1(), decrement.fo1());
    table.write(address, next.fo1());
    hcm::val<index_bits - 1> retained_history = history;
    history = hcm::concat(retained_history.fo1(), outcome.fo1());
    time = std::max(harcom_superuser::ready_time(next), harcom_superuser::ready_time(history));
    harcom_superuser::next_cycle();
  }
};

harcom_gshare::harcom_gshare(O3_CPU* cpu) : branch_predictor(cpu), hardware(std::make_unique<circuit>()) {}
harcom_gshare::harcom_gshare(harcom_gshare&&) noexcept = default;
harcom_gshare::~harcom_gshare() = default;

void harcom_gshare::initialize_branch_predictor() { hcm::panel.make_floorplan(); }

bool harcom_gshare::predict_branch(champsim::address ip) { return hardware->predict(ip); }

void harcom_gshare::last_branch_result(champsim::address ip, champsim::address, bool taken, uint8_t) { hardware->update(ip, taken); }

champsim::energy_model_snapshot harcom_gshare::energy_snapshot() const
{
  return {"HARCOM gshare", "5ceea8358ddbab94e5621e2536530b860cc1e5b7", "0.75 V nominal, 16,384 two-bit SRAM counters, serialized model accesses",
          hcm::panel.energy_fJ(hardware->region), circuit::leakage_watts()};
}
