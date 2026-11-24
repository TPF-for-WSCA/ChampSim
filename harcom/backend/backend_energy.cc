#undef CHAMPSIM_MODULE
#include "../sram_cost.h"
#include "backend_energy.h"
#include "environment.h"
#include "storage.h"

#include <utility>

namespace champsim
{
namespace
{
constexpr std::string_view revision = "5ceea8358ddbab94e5621e2536530b860cc1e5b7";
constexpr auto value_bits = 64;
constexpr auto flop = hcm::flipflops<1>::write_energy_fJ;
constexpr auto gate = hcm::AND<2>.e;
constexpr auto add = hcm::ADD<value_bits>.e;
using event = backend_event;
using harcom::logic_leakage_watts;
using harcom::match_energy;
using harcom::match_leakage_watts;
using harcom::register_leakage_watts;
using harcom::sram_row;

struct term {
  event operation;
  double femtojoules;
};

struct meter {
  std::string instance;
  std::string_view model;
  std::string_view assumptions;
  std::vector<term> terms;
  double leakage_watts;
};

double mux_energy(std::size_t inputs, std::size_t bits)
{
  if (inputs < 2 || bits == 0)
    return 0;
  const auto mux = hcm::mux(inputs, bits, hcm::OUTCAP);
  return mux[0].e + mux[1].e;
}

double mux_leakage(std::size_t inputs, std::size_t bits)
{
  if (inputs < 2 || bits == 0)
    return 0;
  const auto mux = hcm::mux(inputs, bits, hcm::OUTCAP);
  return logic_leakage_watts(mux[0] || mux[1]);
}

double address_select(std::size_t rows)
{
  return rows > 1 ? hcm::anding(storage_bits(rows), hcm::OUTCAP).e : 0;
}

double flag_update(std::size_t rows, std::size_t bits = 1)
{
  return address_select(rows) + static_cast<double>(bits) * flop;
}

meter queue_meter(std::string name, std::size_t depth, std::size_t bits, event in, event out, event squash)
{
  const auto row = sram_row(depth, bits);
  const auto pointer = static_cast<double>(storage_bits(depth + 1)) * (flop + hcm::XOR<2>.e);
  const auto valid = flag_update(depth);
  return {std::move(name), "HARCOM pipeline queue", "One logical SRAM row access per transfer, with local valid bits and pointers",
          {{in, row.write + pointer + valid}, {out, row.read + pointer + valid}, {squash, valid}},
          row.leakage_watts + register_leakage_watts(depth + 2 * storage_bits(depth + 1))};
}

class core_model
{
  const O3_CPU& cpu;
  std::vector<meter> meters;

public:
  explicit core_model(O3_CPU& owner) : cpu(owner)
  {
    owner.reg_allocator.set_energy_activity(owner.backend_activity.emplace());
    const auto physical = cpu.REGISTER_FILE_SIZE;
    const auto phys_bits = std::max<std::size_t>(1, storage_bits(physical));
    constexpr auto logical = RegisterAllocator::logical_registers;
    const auto logical_bits = storage_bits(logical);
    const auto rob = cpu.ROB_SIZE;
    const auto rob_bits = std::max<std::size_t>(1, storage_bits(rob));
    const auto lq = cpu.LQ.size();
    const auto sq = cpu.SQ_SIZE;
    constexpr auto source_slots = NUM_INSTR_SOURCES;
    constexpr auto dest_slots = NUM_INSTR_DESTINATIONS_SPARC;
    constexpr auto operand_slots = source_slots + dest_slots;
    const auto issue_width = static_cast<std::size_t>(std::max(1L, to_underlying(cpu.EXEC_WIDTH)));
    const auto rename_width = static_cast<std::size_t>(std::max(1L, to_underlying(cpu.SCHEDULER_SIZE)));
    const auto dispatch_width = static_cast<std::size_t>(std::max(1L, to_underlying(cpu.DISPATCH_WIDTH)));
    // Address and control only. Effective addresses are produced at issue.
    // Dependency counts and identifiers use the configured hardware index widths.
    const auto decoded_bits = 64 + operand_slots * (logical_bits + 1) + 12;
    auto fetch = queue_meter("instruction fetch queue", cpu.IFETCH_BUFFER_SIZE, decoded_bits, event::fetch_in, event::fetch_out, event::fetch_release);
    fetch.terms.push_back({event::fetch_ready, flag_update(cpu.IFETCH_BUFFER_SIZE, 2)});
    fetch.leakage_watts += register_leakage_watts(2 * cpu.IFETCH_BUFFER_SIZE);
    meters.push_back(std::move(fetch));
    meters.push_back(queue_meter("decode queue", cpu.DECODE_BUFFER_SIZE, decoded_bits, event::decode_in, event::decode_out, event::decode_release));
    meters.push_back(queue_meter("DIB-hit queue", cpu.DIB_HIT_BUFFER_SIZE, decoded_bits, event::dib_in, event::dib_out, event::dib_release));
    meters.push_back(queue_meter("dispatch queue", cpu.DISPATCH_BUFFER_SIZE, decoded_bits, event::dispatch_in, event::dispatch_out, event::dispatch_release));

    const auto rob_state = sram_row(rob, 64 + dest_slots * (phys_bits + 1) + 12);
    const auto rob_destinations = sram_row(rob, dest_slots * (phys_bits + 1));
    meters.push_back({"reorder buffer", "HARCOM ROB and recovery",
                      "PC and destinations in SRAM, local issue/completion/memory state, squash valid bits and redirect control",
                      {{event::rob_in, rob_state.write + flag_update(rob)},
                       {event::rob_rename, rob_destinations.write + flag_update(rob)},
                       {event::rob_issue, flag_update(rob)}, {event::rob_complete, flag_update(rob)},
                       {event::rob_retire, rob_destinations.read + flag_update(rob)},
                       {event::rob_release, flag_update(rob)}, {event::rob_memory_done, flag_update(rob, 4) + hcm::INC<4>.e},
                       {event::recover, static_cast<double>(rob_bits) * flop + hcm::MUX<2, 64>[0].e + hcm::MUX<2, 64>[1].e}},
                      rob_state.leakage_watts + register_leakage_watts(rob + 2 * rob_bits) + mux_leakage(2, 64)});

    const auto rat = sram_row(logical, phys_bits + 1);
    const auto freelist = sram_row(physical, phys_bits);
    const auto rename_metadata = sram_row(physical, logical_bits);
    const auto cursor = static_cast<double>(phys_bits) * flop;
    const auto parallel_checkpoint = static_cast<double>(logical * (phys_bits + 1)) * (flop + hcm::INV.e);
    meters.push_back({"rename and free list", "HARCOM speculative and committed RATs",
                      "Logical-to-physical mappings, free-register FIFO, destination logical IDs and a gated full-map checkpoint",
                      {{event::rat_read, rat.read}, {event::rat_write, rat.write},
                       {event::committed_read, rat.read}, {event::committed_write, rat.write},
                       {event::free_take, freelist.read + cursor}, {event::free_put, freelist.write + cursor},
                       {event::rename_metadata_read, rename_metadata.read}, {event::rename_metadata_write, rename_metadata.write},
                       {event::checkpoint, parallel_checkpoint}, {event::restore, parallel_checkpoint}},
                      2 * rat.leakage_watts + freelist.leakage_watts + rename_metadata.leakage_watts
                          + register_leakage_watts(2 * phys_bits + (cpu.WRONG_PATH ? logical * (phys_bits + 1) : 0))});

    const auto tags = sram_row(rob, source_slots * (phys_bits + 1));
    const auto ready = mux_energy(physical, 1);
    const auto wake = static_cast<double>(phys_bits) * hcm::broadcast_tree(issue_width * source_slots).e;
    meters.push_back({"scheduler and wakeup", "HARCOM scoreboard and issue selection",
                      "Source tags indexed by ROB slot, physical-ready scoreboard, eligible-entry arbitration and a completion-tag broadcast",
                      {{event::rob_rename, tags.write}, {event::select_entry, tags.read + gate * static_cast<double>(issue_width)},
                       {event::ready_read, ready}, {event::ready_write, flag_update(physical)}, {event::wake, wake}},
                      tags.leakage_watts + register_leakage_watts(physical) + mux_leakage(physical, 1)
                          + logic_leakage_watts(hcm::broadcast_tree(issue_width * source_slots), phys_bits)
                          + logic_leakage_watts(hcm::AND<2>, rob * issue_width)});
    const auto prf = sram_row(physical, value_bits);
    meters.push_back({"physical register file", "HARCOM operand storage",
                      "One 64-bit activation per issued source and completed destination, with serialized equivalent SRAM ports",
                      {{event::operand_read, prf.read}, {event::operand_write, prf.write}}, prf.leakage_watts});
    const auto agu_width = static_cast<std::size_t>(to_underlying(cpu.LQ_WIDTH) + to_underlying(cpu.SQ_WIDTH));
    meters.push_back({"execution and address generation", "HARCOM representative 64-bit datapaths",
                      "Scalar add, branch compare and output select per issue lane. One effective-address adder per load/store lane.",
                      {{event::integer_execute, add + mux_energy(2, value_bits)},
                       {event::branch_execute, add + match_energy(value_bits) + mux_energy(2, value_bits)},
                       {event::address_execute, add}},
                      logic_leakage_watts(hcm::ADD<value_bits>, issue_width + agu_width)
                          + static_cast<double>(issue_width) * mux_leakage(2, value_bits) + match_leakage_watts(value_bits, issue_width)});

    const auto load = sram_row(lq, 64 + rob_bits + storage_bits(sq + 1) + 16 + 4);
    const auto store = sram_row(sq, 64 + rob_bits + 16 + 4);
    const auto store_data = sram_row(sq, value_bits);
    const auto load_match_bits = 64 - LOG2_BLOCK_SIZE;
    meters.push_back({"load queue", "HARCOM load address and dependency state",
                      "Allocated address rows, issue reads including retries, cache response CAM and forwarding dependency state",
                      {{event::load_in, load.write + flag_update(lq)}, {event::load_ready, flag_update(lq)},
                       {event::load_issue, load.read}, {event::load_send, flag_update(lq)},
                       {event::load_release, flag_update(lq)}, {event::load_match, match_energy(load_match_bits) + gate},
                       {event::forward_wait, flag_update(lq, storage_bits(sq + 1))}},
                      load.leakage_watts + register_leakage_watts(lq) + match_leakage_watts(load_match_bits, lq)});
    meters.push_back({"store queue and forwarding", "HARCOM store address and data state",
                      "Address CAM on load allocation, store execution and drain, selected data read and bypass on a forward",
                      {{event::store_in, store.write + flag_update(sq)}, {event::store_ready, flag_update(sq)},
                       {event::store_match, match_energy(value_bits) + gate}, {event::store_execute, store_data.write + flag_update(sq)},
                       {event::store_send, store.read + store_data.read}, {event::store_release, flag_update(sq)},
                       {event::forward_data, store_data.read + mux_energy(2, value_bits)}},
                      store.leakage_watts + store_data.leakage_watts + register_leakage_watts(sq + 2 * storage_bits(sq + 1))
                          + match_leakage_watts(value_bits, sq) + mux_leakage(2, value_bits)});

    // HARCOM flip-flop writes already include local clock drive. Only the
    // reference trunk and the input load of each modeled gate are added here.
    const auto gate_groups = dispatch_width + rename_width + issue_width + 8
        + static_cast<std::size_t>(to_underlying(cpu.FETCH_WIDTH) + to_underlying(cpu.DECODE_WIDTH)
                                  + to_underlying(cpu.RETIRE_WIDTH) + to_underlying(cpu.LQ_WIDTH) + to_underlying(cpu.SQ_WIDTH));
    const auto trunk = hcm::wire<4., hcm::DSMAX, 0>(1000, false, hcm::INVCAP * static_cast<double>(gate_groups), false);
    meters.push_back({"clock distribution", "HARCOM reference clock trunk",
                      "One 1 mm trunk with width-scaled gate loads per core, local sequential clock energy included in flip-flop writes",
                      {{event::cycle, 4 * trunk.e}}, logic_leakage_watts(trunk) + logic_leakage_watts(hcm::INV, gate_groups)});
  }

  void sample(std::vector<component_energy_stats>& output) const
  {
    for (const auto& component : meters) {
      double femtojoules = 0;
      for (const auto& term : component.terms)
        femtojoules += static_cast<double>((*cpu.backend_activity)[term.operation]) * term.femtojoules;
      output.push_back({fmt::format("CPU {} {}", cpu.cpu, component.instance), component.model, revision,
                        std::string{component.assumptions}, cpu.clock_period, femtojoules * 1e-15, component.leakage_watts});
    }
  }
};

struct link_model {
  backend_link_configuration config;
  std::size_t request_bits;
  std::size_t write_bits;
  std::size_t response_bits;
  const channel::link_events& activity;
  std::string assumptions;

  link_model(backend_link_configuration shape, double length_um)
      : config(shape),
        // Physical and virtual addresses, PC, ASID, type, flags and prefetch metadata.
        request_bits(64 * 3 + 16 + 4 + 2 + 32 + (shape.byte_mask ? 64 : 0)),
        write_bits(request_bits + (shape.payload == link_payload::cache_line ? 8 * BLOCK_SIZE : 64)),
        response_bits(64 * 2 + 32 + (shape.payload == link_payload::cache_line ? 8 * BLOCK_SIZE
                                       : shape.payload == link_payload::translation ? 64 - LOG2_PAGE_SIZE : 64)),
        activity(shape.connection->link_activity.emplace()),
        assumptions(fmt::format("{:g} um wire, {}/{}/{} bits per request/write/response, reference switching, endpoint FIFOs separate; leakage uses shared request/write lanes and separate response lanes",
                                length_um, request_bits, write_bits, response_bits))
  {}

  double transferred_bits() const
  {
    return static_cast<double>(activity.requests) * static_cast<double>(request_bits)
        + static_cast<double>(activity.writes) * static_cast<double>(write_bits)
        + static_cast<double>(activity.responses) * static_cast<double>(response_bits);
  }
};

class backend_model final : public backend_energy_model
{
  std::vector<core_model> cores;
  std::vector<link_model> links;
  const hcm::circuit wire;

public:
  backend_model(environment& env, double length_um, std::initializer_list<backend_link_configuration> connections)
      : wire(hcm::wire<4., hcm::DSMAX, 0>(length_um, false, hcm::INVCAP, false))
  {
    for (O3_CPU& cpu : env.cpu_view())
      cores.emplace_back(cpu);
    for (const auto& connection : connections)
      links.emplace_back(connection, length_um);
  }

  std::vector<component_energy_stats> energy_snapshots() const override
  {
    std::vector<component_energy_stats> output;
    for (const auto& core : cores)
      core.sample(output);
    for (const auto& link : links)
      output.push_back({std::string(link.config.name), "HARCOM on-chip point-to-point link", revision,
                        link.assumptions, link.config.clock_period, link.transferred_bits() * wire.e * 1e-15,
                        harcom::logic_leakage_watts(wire || hcm::INV, link.write_bits + link.response_bits)});
    return output;
  }

  std::vector<std::string> coverage_gaps() const override
  {
    return {"backend opcode-specific execution, operand switching, and physical multiport layout",
            "full clock tree, interconnect routing and contention, and off-chip PHY"};
  }
};
} // namespace

void configure_harcom_backend(environment& env, double link_length_um, std::initializer_list<backend_link_configuration> links)
{
  env.backend_energy = std::make_unique<backend_model>(env, link_length_um, links);
}
} // namespace champsim
