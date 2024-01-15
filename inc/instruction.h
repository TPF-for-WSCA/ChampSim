#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "circular_buffer.hpp"
#include "trace_instruction.h"

// special registers that help us identify branches
#define REG_STACK_POINTER 6
#define REG_FLAGS 25
#define REG_INSTRUCTION_POINTER 26

// branch types
#define NOT_BRANCH 0
#define BRANCH_DIRECT_JUMP 1
#define BRANCH_INDIRECT 2
#define BRANCH_CONDITIONAL 3
#define BRANCH_DIRECT_CALL 4
#define BRANCH_INDIRECT_CALL 5
#define BRANCH_RETURN 6
#define BRANCH_OTHER 7

struct ooo_model_instr {
  uint64_t instr_id = 0, ip = 0, event_cycle = 0, size = 0;
  int indirect_branches = 0;

  bool is_branch = 0, is_memory = 0, branch_taken = 0, branch_mispredicted = 0, fake_instr = 0, source_added[NUM_INSTR_SOURCES] = {},
       destination_added[NUM_INSTR_DESTINATIONS_SPARC] = {};

  uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

  uint8_t branch_type = NOT_BRANCH;
  uint64_t branch_target = 0;

  uint8_t translated = 0, fetched = 0, decoded = 0, scheduled = 0, executed = 0;
  int num_reg_ops = 0, num_mem_ops = 0, num_reg_dependent = 0;

  uint8_t destination_registers[NUM_INSTR_DESTINATIONS_SPARC] = {}; // output registers

  uint8_t source_registers[NUM_INSTR_SOURCES] = {}; // input registers

  // these are indices of instructions in the ROB that depend on me
  std::vector<champsim::circular_buffer<ooo_model_instr>::iterator> registers_instrs_depend_on_me, memory_instrs_depend_on_me;

  // memory addresses that may cause dependencies between instructions
  uint64_t instruction_pa = 0;
  uint64_t destination_memory[NUM_INSTR_DESTINATIONS_SPARC] = {}; // output memory
  uint64_t source_memory[NUM_INSTR_SOURCES] = {};                 // input memory

  std::array<std::vector<LSQ_ENTRY>::iterator, NUM_INSTR_SOURCES> lq_index = {};
  std::array<std::vector<LSQ_ENTRY>::iterator, NUM_INSTR_DESTINATIONS_SPARC> sq_index = {};

  ooo_model_instr() = default;

  ooo_model_instr(uint8_t cpu, input_instr instr)
  {
    std::copy(std::begin(instr.destination_registers), std::end(instr.destination_registers), std::begin(this->destination_registers));
    std::copy(std::begin(instr.destination_memory), std::end(instr.destination_memory), std::begin(this->destination_memory));
    std::copy(std::begin(instr.source_registers), std::end(instr.source_registers), std::begin(this->source_registers));
    std::copy(std::begin(instr.source_memory), std::end(instr.source_memory), std::begin(this->source_memory));

    this->ip = instr.ip;

    this->is_branch = instr.is_branch;
    this->branch_taken = instr.branch_taken;

    asid[0] = cpu;
    asid[1] = cpu;

    extern uint8_t knob_intel;
    if (knob_intel) {
      return;
    }
    extern int8_t knob_ip_offset;

    this->ip += knob_ip_offset;
    assert(this->ip % 4 == 0 || this->ip % 2 == 0); // check if it an ARM trace
    this->size = 4;                                 // average for x86 (rounded up)
    if (this->ip % 2 == 0 and not(this->ip % 4 == 0))
      this->size = 2; // compressed instr.
  }

  ooo_model_instr(uint8_t cpu, cloudsuite_instr instr)
  {
    std::copy(std::begin(instr.destination_registers), std::end(instr.destination_registers), std::begin(this->destination_registers));
    std::copy(std::begin(instr.destination_memory), std::end(instr.destination_memory), std::begin(this->destination_memory));
    std::copy(std::begin(instr.source_registers), std::end(instr.source_registers), std::begin(this->source_registers));
    std::copy(std::begin(instr.source_memory), std::end(instr.source_memory), std::begin(this->source_memory));

    this->ip = instr.ip;

    this->is_branch = instr.is_branch;
    this->branch_taken = instr.branch_taken;

    extern uint8_t knob_intel;
    std::copy(std::begin(instr.asid), std::begin(instr.asid), std::begin(this->asid));
    if (knob_intel) {
      return;
    }
    extern int8_t knob_ip_offset;

    this->ip += knob_ip_offset;
    assert(this->ip % 4 == 0 || this->ip % 2 == 0); // check if it an ARM trace
    this->size = 4;                                 // average for x86 (rounded up)
    if (this->ip % 2 == 0 and not(this->ip % 4 == 0))
      this->size = 2; // compressed instr.
  }

  ooo_model_instr(uint8_t cpu, pt_instr instr)
  {
    std::copy(std::begin(instr.destination_registers), std::end(instr.destination_registers), std::begin(this->destination_registers));
    std::copy(std::begin(instr.destination_memory), std::end(instr.destination_memory), std::begin(this->destination_memory));
    std::copy(std::begin(instr.source_registers), std::end(instr.source_registers), std::begin(this->source_registers));
    // std::copy(std::begin(instr.source_memory), std::end(instr.source_memory), std::begin(this->source_memory));

    auto category = xed_decoded_inst_get_category(&instr.decoded_instruction);
    this->is_branch = true;
    switch (category) {
    case XED_CATEGORY_COND_BR:
      this->branch_type = BRANCH_CONDITIONAL;
      break;
    case XED_CATEGORY_UNCOND_BR:
      if (xed3_operand_get_brdisp_width(&instr.decoded_instruction))
        this->branch_type = BRANCH_DIRECT_JUMP;
      else {
        this->branch_type = BRANCH_INDIRECT;
        indirect_branches++;
      }
      break;
    case XED_CATEGORY_CALL:
      if (xed3_operand_get_brdisp_width(&instr.decoded_instruction))
        this->branch_type = BRANCH_DIRECT_CALL;
      else {
        this->branch_type = BRANCH_INDIRECT_CALL;
        indirect_branches++;
      }
      break;
    case XED_CATEGORY_RET:
      this->branch_type = BRANCH_RETURN;
      break;
    default:
      this->branch_type = NOT_BRANCH;
      this->is_branch = NOT_BRANCH;
      break;
    }
    this->ip = instr.pc;
    this->branch_taken = instr.branch_taken;
    this->size = instr.size;
    if (instr.mem_refs > 0)
      this->is_memory = 1;

    asid[0] = cpu;
    asid[1] = cpu;
  }
};

#endif
