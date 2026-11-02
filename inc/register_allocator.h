#include <array>
#include <cstdint>
#include <list>
#include <optional>
#include <queue>

#ifndef REG_ALLOC_H
#define REG_ALLOC_H

#include "instruction.h"

struct physical_register {
  uint16_t arch_reg_index;
  uint64_t producing_instruction_id;
  bool valid; // has the producing instruction committed yet?
  bool busy;  // is this register in use anywhere in the pipeline?
};

class RegisterAllocator
{
public:
  // Trace registers occupy [0, 255]. Inference has a separate logical namespace.
  static constexpr PHYSICAL_REGISTER_ID INFERRED_LOAD_REGISTER = 256, INFERRED_ALU_REGISTER = 257;

private:
  using rename_table = std::array<PHYSICAL_REGISTER_ID, INFERRED_ALU_REGISTER + 1>;
  rename_table frontend_RAT, backend_RAT;
  std::optional<rename_table> speculation_checkpoint;
  std::vector<PHYSICAL_REGISTER_ID> speculative_registers;
  std::queue<PHYSICAL_REGISTER_ID> free_registers;
  std::vector<physical_register> physical_register_file;

public:
  RegisterAllocator(size_t num_physical_registers);
  PHYSICAL_REGISTER_ID rename_dest_register(int16_t reg, champsim::program_ordered<ooo_model_instr>::id_type producer_id);
  PHYSICAL_REGISTER_ID rename_src_register(int16_t reg);
  void complete_dest_register(PHYSICAL_REGISTER_ID physreg);
  void retire_dest_register(PHYSICAL_REGISTER_ID physreg);
  void free_register(PHYSICAL_REGISTER_ID physreg);
  bool isValid(PHYSICAL_REGISTER_ID physreg) const;
  bool isAllocated(PHYSICAL_REGISTER_ID archreg) const;
  unsigned long count_free_registers() const;
  int count_reg_dependencies(const ooo_model_instr& instr) const;
  void reset_frontend_RAT();
  void begin_speculation();
  void squash_speculation();
  void print_deadlock();
};
#endif
