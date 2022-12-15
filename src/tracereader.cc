#include "tracereader.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <zlib.h>

extern "C" {
#include <xed/xed-interface.h>
}

#define GZ_BUFFER_SIZE 80

static bool xedInitialized = false;
tracereader::tracereader(uint8_t cpu, std::string _ts) : cpu(cpu), trace_string(_ts)
{
  std::string last_dot = trace_string.substr(trace_string.find_last_of("."));

  if (trace_string.substr(0, 4) == "http") {
    // Check file exists
    char testfile_command[4096];
    sprintf(testfile_command, "wget -q --spider %s", trace_string.c_str());
    FILE* testfile = popen(testfile_command, "r");
    if (pclose(testfile)) {
      std::cerr << "TRACE FILE NOT FOUND" << std::endl;
      assert(0);
    }
    cmd_fmtstr = "wget -qO- -o /dev/null %2$s | %1$s -dc";
  } else {
    std::ifstream testfile(trace_string);
    if (!testfile.good()) {
      std::cerr << "TRACE FILE NOT FOUND" << std::endl;
      assert(0);
    }
    cmd_fmtstr = "%1$s -dc %2$s";
  }

  if (last_dot[1] == 'g') // gzip format
    decomp_program = "gzip";
  else if (last_dot[1] == 'x') // xz
    decomp_program = "xz";
  else {
    std::cout << "ChampSim does not support traces other than gz or xz compression!" << std::endl;
    assert(0);
  }

  open(trace_string);
}

tracereader::~tracereader() { close(); }

template <typename T>
ooo_model_instr tracereader::read_single_instr()
{
  T trace_read_instr;

  while (!fread(&trace_read_instr, sizeof(T), 1, trace_file)) {
    // reached end of file for this trace
    std::cout << "*** Reached end of trace: " << trace_string << std::endl;

    // close the trace file and re-open it
    close();
    open(trace_string);
  }

  // copy the instruction into the performance model's instruction format
  ooo_model_instr retval(cpu, trace_read_instr);
  return retval;
}

void tracereader::open(std::string trace_string)
{
  char gunzip_command[4096];
  sprintf(gunzip_command, cmd_fmtstr.c_str(), decomp_program.c_str(), trace_string.c_str());
  trace_file = popen(gunzip_command, "r");
  if (trace_file == NULL) {
    std::cerr << std::endl << "*** CANNOT OPEN TRACE FILE: " << trace_string << " ***" << std::endl;
    assert(0);
  }
}

void tracereader::close()
{
  if (trace_file != NULL) {
    pclose(trace_file);
  }
}

class cloudsuite_tracereader : public tracereader
{
  ooo_model_instr last_instr;
  bool initialized = false;

public:
  cloudsuite_tracereader(uint8_t cpu, std::string _tn) : tracereader(cpu, _tn) {}

  ooo_model_instr get()
  {
    ooo_model_instr trace_read_instr = read_single_instr<cloudsuite_instr>();

    if (!initialized) {
      last_instr = trace_read_instr;
      initialized = true;
    }

    last_instr.branch_target = trace_read_instr.ip;
    ooo_model_instr retval = last_instr;

    last_instr = trace_read_instr;
    return retval;
  }
};

class input_tracereader : public tracereader
{
  ooo_model_instr last_instr;
  bool initialized = false;

public:
  input_tracereader(uint8_t cpu, std::string _tn) : tracereader(cpu, _tn) {}

  ooo_model_instr get()
  {
    ooo_model_instr trace_read_instr = read_single_instr<input_instr>();

    if (!initialized) {
      last_instr = trace_read_instr;
      initialized = true;
    }

    last_instr.branch_target = trace_read_instr.ip;
    ooo_model_instr retval = last_instr;

    last_instr = trace_read_instr;
    return retval;
  }
};

class pt_tracereader : public tracereader
{
  ooo_model_instr last_instr;
  bool initialized = false;
  gzFile trace_file;

private:
  void reopen_file()
  {
    if (trace_file != NULL) {
      gzclose(trace_file);
    }
    trace_file = gzopen(trace_string.c_str(), "rb");
    if (!trace_file) {
      std::cerr << "\nCANNT OPEN TRACE FILE " << trace_string << std::endl;
      assert(0);
    }
  }

  ooo_model_instr translate_instr(pt_instr ptinstr)
  {
    ooo_model_instr inst;
    int num_reg_ops = 0;
    int num_mem_ops = 0;
    inst.ip = ptinstr.pc;
    // inst.size = ptinstr.size;
  }

public:
  explicit pt_tracereader(uint8_t cpu, std::string file_name) : tracereader(cpu, file_name)
  {
    this->reopen_file();
    if (!xedInitialized) {
      xed_tables_init();
      xedInitialized = true;
    }
  }

  ~pt_tracereader() { gzclose(trace_file); }

  ooo_model_instr get()
  {
    char buffer[GZ_BUFFER_SIZE];
    pt_instr trace_read_instr_pt;
    do {
      // gzgets reads at max one line (\n)
      if (gzgets(trace_file, buffer, GZ_BUFFER_SIZE) == Z_NULL) {
        std::cout << "REACHED END OF TRACE: " << trace_string << std::endl;
        // reopen to continue until sim limits reached
        this->reopen_file();
        gzgets(trace_file, buffer, GZ_BUFFER_SIZE);
      }
      trace_read_instr_pt = pt_instr(buffer);
    } while (trace_read_instr_pt.pc == 0);

    xed_decoded_inst_t raw_pt_inst;
    xed_decoded_inst_zero(&raw_pt_inst);
    xed_decoded_inst_set_mode(&raw_pt_inst, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b);
    xed_error_enum_t xed_error = xed_decode(&raw_pt_inst, trace_read_instr_pt.inst_bytes.data(), trace_read_instr_pt.size);
    if (xed_error != XED_ERROR_NONE) {
      std::cerr << "DECODING OF PT INSTR FAILED" << std::endl;
      assert(0);
    }

    uint32_t numOperands = xed_decoded_inst_noperands(&raw_pt_inst);
    xed_iclass_enum_t opcode = xed_decoded_inst_get_iclass(&raw_pt_inst);
    uint32_t inRegIdx = 0, outRegIdx = 0;
    uint32_t inMemIdx = 0, outMemIdx = 0;
    for (uint32_t opIdx = 0; opIdx < numOperands; opIdx++) {
      bool readOp, writeOp;
      switch (xed_decoded_inst_operand_action(&raw_pt_inst, opIdx)) {
      case XED_OPERAND_ACTION_RW:
      case XED_OPERAND_ACTION_RCW:
      case XED_OPERAND_ACTION_CRW:
        readOp = true;
        writeOp = true;
        break;
      case XED_OPERAND_ACTION_R:
      case XED_OPERAND_ACTION_CR:
        readOp = true;
        break;
      case XED_OPERAND_ACTION_W:
      case XED_OPERAND_ACTION_CW:
        writeOp = true;
        break;
      default:
        assert(0);
      }

      const xed_operand_t* operand = xed_inst_operand(raw_pt_inst._inst, opIdx);
      xed_operand_enum_t opName = xed_operand_name(operand);
      if (xed_operand_is_register(opName)) {
        if (xed_decoded_inst_get_iclass(&raw_pt_inst) == XED_ICLASS_CALL_NEAR && outRegIdx > 0) {
          std::cerr << " we got more than one out reg for a call: " << opName << std::endl;
          continue; // How can a near call have two out registers?
        }
        auto reg = xed_decoded_inst_get_reg(&raw_pt_inst, opName);
        reg = xed_get_largest_enclosing_register(reg);

        if (readOp) {
          trace_read_instr_pt.source_registers[inRegIdx++] = reg;
        }
        if (writeOp) {
          trace_read_instr_pt.destination_registers[outRegIdx++] = reg;
        }

        assert(inRegIdx + outRegIdx <= 3);
      } else if (opName == XED_OPERAND_MEM0 || opName == XED_OPERAND_MEM1) {

        if (readOp) {
          trace_read_instr_pt.source_memory[inMemIdx++] = 1;
        }
        if (writeOp) {
        }
      }
    }

    ooo_model_instr arch_instr(cpu, trace_read_instr_pt);
    // Check if we need to handle branching here already ( Branch information )
    if (!initialized) {
      last_instr = arch_instr;
      initialized = true;
    }

    last_instr.branch_taken = last_instr.ip + last_instr.size != arch_instr.ip;
    if (last_instr.is_branch && last_instr.branch_taken) {
      last_instr.branch_target = arch_instr.ip;
    }
    auto retval = last_instr;
    last_instr = arch_instr;
    return retval;
  }
};

tracereader* get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite, bool is_pt)
{
  if (is_cloudsuite) {
    return new cloudsuite_tracereader(cpu, fname);
  } else if (is_pt) {
    return new pt_tracereader(cpu, fname);
  } else {
    return new input_tracereader(cpu, fname);
  }
}
