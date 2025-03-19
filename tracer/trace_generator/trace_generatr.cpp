#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "../../inc/trace_instruction.h"
#include "./clipp.h"

using namespace clipp;
using trace_instr_format_t = input_instr;

uint64_t instrCount = 0;

std::ofstream outfile;

trace_instr_format_t curr_instr;

int main(int argc, char* argv[])
{
  std::string outFilePath = "";
  uint64_t num_instructions = 2048;

  auto cli = (value("output trace file", outFilePath), opt_value("-t", num_instructions));

  parse(argc, argv, cli);

  outfile.open(outFilePath.c_str(), std::ios_base::binary | std::ios_base::trunc);
  int double_iteration = 0;
  while (double_iteration < 2) {
    for (int i = 0; i < num_instructions / 2; i++) {
      curr_instr = {};
      curr_instr.ip = (i << 2) + (i << 2 << 9) + 0x88880000; // One entry per set for each region
      curr_instr.is_branch = 1;
      curr_instr.branch_taken = 1;

      curr_instr.source_registers[0] = champsim::REG_INSTRUCTION_POINTER;
      curr_instr.source_registers[0] = champsim::REG_FLAGS;

      curr_instr.destination_registers[0] = champsim::REG_INSTRUCTION_POINTER;
      typename decltype(outfile)::char_type buf[sizeof(trace_instr_format_t)];
      std::memcpy(buf, &curr_instr, sizeof(trace_instr_format_t));
      outfile.write(buf, sizeof(trace_instr_format_t));
    }
    double_iteration++;
  }

  outfile.close();
}