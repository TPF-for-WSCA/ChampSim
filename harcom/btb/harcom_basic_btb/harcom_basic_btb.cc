#include <bitset>
#include <optional>

// The adapter uses core internals in addition to the module interface.
#undef CHAMPSIM_MODULE
#include "../../ittage_cost.h"
#include "harcom_basic_btb.h"
#include "ooo_cpu.h"

namespace
{
constexpr std::string_view harcom_revision = "5ceea8358ddbab94e5621e2536530b860cc1e5b7";

struct associative_cost {
  double lookup;
  double promote;
  double fill;
};

template <std::size_t Sets, std::size_t TagBits, std::size_t PayloadBits>
constexpr associative_cost eight_way_cost()
{
  using way = hcm::sram<Sets, TagBits + 1 + PayloadBits>;
  using ranks = hcm::sram<Sets, 8 * 3>;
  // All eight ways are read in parallel. A fill writes one way and one LRU row.
  constexpr auto compare = hcm::EQUAL<TagBits>.e + hcm::AND<2>.e;
  constexpr auto lookup = 8 * (way::EREAD + compare) + hcm::OR<8>.e;
  constexpr auto promote = ranks::EREAD + ranks::EWRITE;
  if constexpr (PayloadBits > 0) {
    constexpr auto mux = hcm::MUX<8, PayloadBits>;
    return {lookup + mux[0].e + mux[1].e, promote, promote + way::EWRITE};
  }
  return {lookup, promote, lookup + promote + way::EWRITE};
}

constexpr auto dib_cost = eight_way_cost<32, 55, 0>();
constexpr auto direct_1024_cost = eight_way_cost<1024, 52, 67>();
constexpr auto direct_512_cost = eight_way_cost<512, 53, 67>();

// One bank per fetch-block slot, with a PC, target and three prediction flags per word.
using ftq_slot = hcm::sram<24, 64 + 64 + 3>;
constexpr auto ftq_cursor = hcm::flipflops<3>::write_energy_fJ + hcm::INC<3>.e;
constexpr auto ftq_block_pointer = hcm::flipflops<5>::write_energy_fJ + hcm::INC<5>.e;
constexpr auto pc_select = hcm::MUX<2, 64>;
constexpr auto next_pc = hcm::INC<62>.e + hcm::NEQ<64>.e + hcm::AND<2>.e + pc_select[0].e + pc_select[1].e;

using ras_entries = hcm::sram<return_stack::max_size, 64>;
using call_sizes = hcm::sram<return_stack::num_call_size_trackers, 4>;
constexpr auto ras_pointer = hcm::flipflops<13>::write_energy_fJ + hcm::INC<6>.e;
constexpr auto call_size_valid = hcm::MUX<return_stack::num_call_size_trackers, 1>;
constexpr auto call_size_valid_read = call_size_valid[0].e + call_size_valid[1].e;
constexpr auto call_size_valid_write = hcm::decoder<return_stack::num_call_size_trackers>.e + hcm::flipflops<1>::write_energy_fJ;
} // namespace

class harcom_basic_btb::frontend_model final : public champsim::frontend_energy_model
{
  const bool dib_supported;
  const bool fdip_enabled;
  const bool ftq_supported;
  const bool ideal_frontend;
  const bool ideal_targets;
  std::optional<associative_cost> direct_cost;
  double dib_energy = 0;
  double ftq_energy = 0;
  double fdip_energy = 0;
  double direct_energy = 0;
  double indirect_energy = 0;
  double ras_energy = 0;
  std::bitset<return_stack::num_call_size_trackers> saved_call_sizes;

  void record_ras(champsim::frontend_event event, std::size_t argument)
  {
    if (ideal_targets)
      return;
    using champsim::frontend_event;
    switch (event) {
    case frontend_event::ras_push:
      ras_energy += ras_entries::EWRITE + ras_pointer;
      break;
    case frontend_event::ras_pop:
      ras_energy += ras_pointer;
      break;
    case frontend_event::ras_calibration_pop:
      ras_energy += ras_pointer + ras_entries::EREAD + hcm::SUB<64>.e + hcm::GTE<64>.e;
      break;
    case frontend_event::ras_return:
    case frontend_event::ras_speculative_return:
      ras_energy += ras_entries::EREAD + call_sizes::EREAD + hcm::ADD<64>.e;
      // The checkpoint bitmap selects either the live or shadow SRAM.
      if (event == frontend_event::ras_speculative_return)
        ras_energy += call_size_valid_read;
      break;
    case frontend_event::ras_calibration_write:
      ras_energy += call_sizes::EWRITE;
      if (fdip_enabled) {
        ras_energy += call_size_valid_read;
        if (!saved_call_sizes.test(argument)) {
          saved_call_sizes.set(argument);
          ras_energy += call_sizes::EREAD + call_sizes::EWRITE + call_size_valid_write;
        }
      }
      break;
    case frontend_event::ras_checkpoint:
      saved_call_sizes.reset();
      ras_energy += static_cast<double>(argument) * (ras_entries::EREAD + ras_entries::EWRITE)
          + hcm::flipflops<13>::write_energy_fJ + hcm::flipflops<return_stack::num_call_size_trackers>::write_energy_fJ;
      break;
    default:
      break;
    }
  }

public:
  frontend_model(const O3_CPU& cpu, uint8_t tag_bits)
      : dib_supported(cpu.DIB_SETS == 32 && cpu.DIB_WAYS == 8 && cpu.DIB_WINDOW == 16), fdip_enabled(cpu.uses_fdip()),
        ftq_supported(fdip_enabled && cpu.FDIP_FTQ_SIZE == 24), ideal_frontend(cpu.IDEAL_CBP || cpu.IDEAL_IBP || cpu.IDEAL_BTB),
        ideal_targets(cpu.IDEAL_IBP || cpu.IDEAL_BTB)
  {
    if (cpu.BTB_WAYS != 8 || ideal_targets)
      return;
    if (cpu.BTB_SETS == 1024 && tag_bits == 52)
      direct_cost = direct_1024_cost;
    if (cpu.BTB_SETS == 512 && tag_bits == 53)
      direct_cost = direct_512_cost;
  }

  void record(champsim::frontend_event event, std::size_t count) override
  {
    using champsim::frontend_event;
    switch (event) {
    case frontend_event::dib_lookup:
      if (dib_supported) dib_energy += dib_cost.lookup;
      break;
    case frontend_event::dib_hit:
      if (dib_supported) dib_energy += dib_cost.promote;
      break;
    case frontend_event::dib_fill:
      if (dib_supported) dib_energy += dib_cost.fill;
      break;
    case frontend_event::ftq_read:
      if (ftq_supported) ftq_energy += ftq_slot::EREAD + hcm::EQUAL<64>.e;
      break;
    case frontend_event::ftq_write:
      if (ftq_supported) ftq_energy += static_cast<double>(count) * ftq_slot::EWRITE;
      break;
    case frontend_event::ftq_push:
      if (ftq_supported) ftq_energy += ftq_block_pointer + hcm::flipflops<4>::write_energy_fJ;
      break;
    case frontend_event::ftq_consume:
      if (ftq_supported) ftq_energy += ftq_cursor;
      break;
    case frontend_event::ftq_pop:
      if (ftq_supported) ftq_energy += ftq_block_pointer;
      break;
    case frontend_event::ftq_redirect:
      if (ftq_supported) ftq_energy += hcm::flipflops<13>::write_energy_fJ;
      if (fdip_enabled) fdip_energy += hcm::flipflops<64>::write_energy_fJ;
      break;
    case frontend_event::fdip_slot:
      if (fdip_enabled) fdip_energy += static_cast<double>(count) * (next_pc + hcm::flipflops<64>::write_energy_fJ);
      break;
    case frontend_event::btb_lookup:
      if (direct_cost) direct_energy += direct_cost->lookup;
      break;
    case frontend_event::btb_promote:
      if (direct_cost) direct_energy += direct_cost->promote;
      break;
    case frontend_event::btb_fill:
      if (direct_cost) direct_energy += direct_cost->fill;
      break;
    case frontend_event::ras_push:
    case frontend_event::ras_pop:
    case frontend_event::ras_calibration_pop:
    case frontend_event::ras_return:
    case frontend_event::ras_speculative_return:
    case frontend_event::ras_calibration_write:
    case frontend_event::ras_checkpoint:
      record_ras(event, count);
      break;
    default:
      if (!ideal_targets) indirect_energy += champsim::harcom::ittage_energy(event, count);
      break;
    }
  }

  [[nodiscard]] std::vector<champsim::energy_model_snapshot> energy_snapshots() const override
  {
    std::vector<champsim::energy_model_snapshot> snapshots;
    if (dib_supported)
      snapshots.push_back({"DIB", harcom_revision, "32 sets, 8 ways, 16-byte window, tag and LRU row arrays", dib_energy});
    if (ftq_supported)
      snapshots.push_back({"FDIP FTQ", harcom_revision, "24 blocks, eight slot banks, 131 bits per prediction", ftq_energy});
    if (fdip_enabled)
      snapshots.push_back({"FDIP next-PC logic", harcom_revision, "sequential PC, target selection and PC register", fdip_energy});
    if (direct_cost)
      snapshots.push_back({"basic BTB direct table", harcom_revision, "eight parallel SRAM ways, full tags, 64-bit targets and LRU row array", direct_energy});
    if (!ideal_targets)
      snapshots.push_back({"basic BTB ITTAGE", harcom_revision, "parallel base and tagged SRAMs, resettable useful bits and shift-register history", indirect_energy});
    if (!ideal_targets)
      snapshots.push_back({"basic BTB return-address stack", harcom_revision,
                           fdip_enabled ? "64-entry stacks, live stack entries copied on an FDIP checkpoint, call-size SRAM with copy-on-write shadow"
                                        : "64-entry return stack and four-bit call-size SRAM",
                           ras_energy});
    return snapshots;
  }

  [[nodiscard]] std::vector<std::string> coverage_gaps() const override
  {
    std::vector<std::string> gaps;
    if (!dib_supported)
      gaps.emplace_back("DIB geometry outside 32 sets, eight ways and a 16-byte window");
    if (fdip_enabled && !ftq_supported)
      gaps.emplace_back("FDIP FTQ geometry outside 24 fetch blocks");
    if (ideal_frontend)
      gaps.emplace_back("ideal frontend prediction oracle");
    if (!direct_cost && !ideal_targets)
      gaps.emplace_back("basic BTB direct table geometry outside eight ways and 512 or 1024 sets with full tags");
    if (ideal_targets)
      gaps.emplace_back("basic BTB direct table, ITTAGE and return stack under ideal target prediction");
    return gaps;
  }
};

harcom_basic_btb::harcom_basic_btb(O3_CPU* owner) : basic_btb(owner), cpu(owner) {}
harcom_basic_btb::harcom_basic_btb(harcom_basic_btb&&) noexcept = default;
harcom_basic_btb::~harcom_basic_btb() = default;

void harcom_basic_btb::initialize_btb()
{
  basic_btb::initialize_btb();
  if (cpu != nullptr) {
    model = std::make_unique<frontend_model>(*cpu, direct_tag_bits());
    cpu->frontend_energy = model.get();
    set_energy_model(model.get());
  }
}
