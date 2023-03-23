#ifndef TRACE_INSTRUCTION_H
#define TRACE_INSTRUCTION_H

#include <limits>

extern "C" {
#include <xed/xed-interface.h>
}

// instruction format
constexpr std::size_t NUM_INSTR_DESTINATIONS_SPARC = 4;
constexpr std::size_t NUM_INSTR_DESTINATIONS_X86 = 3;
constexpr std::size_t NUM_INSTR_DESTINATIONS = 2;
constexpr std::size_t NUM_INSTR_SOURCES = 4;

class LSQ_ENTRY;

struct input_instr {
  // instruction pointer or PC (Program Counter)
  unsigned long long ip = 0;

  // branch info
  unsigned char is_branch = 0;
  unsigned char branch_taken = 0;

  unsigned char destination_registers[NUM_INSTR_DESTINATIONS] = {}; // output registers
  unsigned char source_registers[NUM_INSTR_SOURCES] = {};           // input registers

  unsigned long long destination_memory[NUM_INSTR_DESTINATIONS] = {}; // output memory
  unsigned long long source_memory[NUM_INSTR_SOURCES] = {};           // input memory
};

struct cloudsuite_instr {
  // instruction pointer or PC (Program Counter)
  unsigned long long ip = 0;

  // branch info
  unsigned char is_branch = 0;
  unsigned char branch_taken = 0;

  unsigned char destination_registers[NUM_INSTR_DESTINATIONS_SPARC] = {}; // output registers
  unsigned char source_registers[NUM_INSTR_SOURCES] = {};                 // input registers

  unsigned long long destination_memory[NUM_INSTR_DESTINATIONS_SPARC] = {}; // output memory
  unsigned long long source_memory[NUM_INSTR_SOURCES] = {};                 // input memory

  unsigned char asid[2] = {std::numeric_limits<unsigned char>::max(), std::numeric_limits<unsigned char>::max()};
};

struct pt_instr {
  uint64_t pc = 0;
  uint8_t size = 0;

  xed_decoded_inst_t decoded_instruction;

  unsigned char is_branch = 0;
  unsigned char branch_taken = 0;

  int mem_refs = 0;

  unsigned char destination_registers[NUM_INSTR_DESTINATIONS_X86] = {};
  unsigned char source_registers[NUM_INSTR_SOURCES] = {};

  unsigned long long destination_memory[NUM_INSTR_DESTINATIONS_X86] = {}; // output memory
  unsigned long long source_memory[NUM_INSTR_SOURCES] = {};               // input memory

  std::vector<uint8_t> inst_bytes;

  pt_instr() = default;

  pt_instr(char* buffer)
  {
    inst_bytes.resize(16, 0); // allocate enough space for "MAX" x86 instr
    std::vector<uint64_t> tmp;
    char* end;
    for (auto i = strtoul(buffer, &end, 16); buffer != end; i = strtoul(buffer, &end, 16)) {
      buffer = end;
      tmp.push_back(i);
    }
    if (tmp.size() < 3 || tmp.size() != tmp[1] + 2) {
      // decoding error: we cannot read size, pc and inst bytes or inst bytes do
      // not match reported size
      return;
    }
    pc = tmp[0];
    size = tmp[1];
    for (size_t i = 2; i < tmp.size(); i++) {
      inst_bytes[i - 2] = tmp[i];
    }
  };
};
#endif
