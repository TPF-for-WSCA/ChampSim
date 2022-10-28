#ifndef TRACE_INSTRUCTION_H
#define TRACE_INSTRUCTION_H

#include <limits>

// instruction format
constexpr std::size_t NUM_INSTR_DESTINATIONS_SPARC = 4;
constexpr std::size_t NUM_INSTR_DESTINATIONS = 2;
constexpr std::size_t NUM_INSTR_SOURCES = 4;

extern "C" {
#include <xed/xed-interface.h>
}

class pt_instr
{
public:
  uint64_t pc = 0;
  uint8_t size = 0;
  std::vector<uint8_t> inst_bytes;

  pt_instr() = default;

  explicit pt_instr(char* buffer)
  {
    inst_bytes.resize(16, 0);
    auto x = buffer;
    std::vector<uint64_t> curr;
    char* end;
    for (auto i = strtoul(x, &end, 16); x != end; i = strtoul(x, &end, 16)) {
      x = end;
      curr.push_back(i);
    }
    if (curr.size() < 3 || curr.size() != curr[1] + 2) {
      size = 0;
      return;
    }
    pc = curr[0];
    size = curr[1];
    for (size_t i = 2; i < curr.size(); i++) {
      inst_bytes[i - 2] = curr[i];
    }
  }
};

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

#endif
