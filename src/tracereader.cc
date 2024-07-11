#include "tracereader.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <zlib.h>

extern "C" {
#include <xed/xed-interface.h>
}

#define GZ_BUFFER_SIZE 50
extern uint8_t knob_intel;
extern uint8_t knob_stop_at_completion;
extern int8_t knob_ip_offset;

static const char* XED_REG_STR[] = {
    "XED_REG_INVALID",   "XED_REG_BNDCFGU", "XED_REG_BNDSTATUS", "XED_REG_BND0",       "XED_REG_BND1",       "XED_REG_BND2",      "XED_REG_BND3",
    "XED_REG_CR0",       "XED_REG_CR1",     "XED_REG_CR2",       "XED_REG_CR3",        "XED_REG_CR4",        "XED_REG_CR5",       "XED_REG_CR6",
    "XED_REG_CR7",       "XED_REG_CR8",     "XED_REG_CR9",       "XED_REG_CR10",       "XED_REG_CR11",       "XED_REG_CR12",      "XED_REG_CR13",
    "XED_REG_CR14",      "XED_REG_CR15",    "XED_REG_DR0",       "XED_REG_DR1",        "XED_REG_DR2",        "XED_REG_DR3",       "XED_REG_DR4",
    "XED_REG_DR5",       "XED_REG_DR6",     "XED_REG_DR7",       "XED_REG_FLAGS",      "XED_REG_EFLAGS",     "XED_REG_RFLAGS",    "XED_REG_AX",
    "XED_REG_CX",        "XED_REG_DX",      "XED_REG_BX",        "XED_REG_SP",         "XED_REG_BP",         "XED_REG_SI",        "XED_REG_DI",
    "XED_REG_R8W",       "XED_REG_R9W",     "XED_REG_R10W",      "XED_REG_R11W",       "XED_REG_R12W",       "XED_REG_R13W",      "XED_REG_R14W",
    "XED_REG_R15W",      "XED_REG_EAX",     "XED_REG_ECX",       "XED_REG_EDX",        "XED_REG_EBX",        "XED_REG_ESP",       "XED_REG_EBP",
    "XED_REG_ESI",       "XED_REG_EDI",     "XED_REG_R8D",       "XED_REG_R9D",        "XED_REG_R10D",       "XED_REG_R11D",      "XED_REG_R12D",
    "XED_REG_R13D",      "XED_REG_R14D",    "XED_REG_R15D",      "XED_REG_RAX",        "XED_REG_RCX",        "XED_REG_RDX",       "XED_REG_RBX",
    "XED_REG_RSP",       "XED_REG_RBP",     "XED_REG_RSI",       "XED_REG_RDI",        "XED_REG_R8",         "XED_REG_R9",        "XED_REG_R10",
    "XED_REG_R11",       "XED_REG_R12",     "XED_REG_R13",       "XED_REG_R14",        "XED_REG_R15",        "XED_REG_AL",        "XED_REG_CL",
    "XED_REG_DL",        "XED_REG_BL",      "XED_REG_SPL",       "XED_REG_BPL",        "XED_REG_SIL",        "XED_REG_DIL",       "XED_REG_R8B",
    "XED_REG_R9B",       "XED_REG_R10B",    "XED_REG_R11B",      "XED_REG_R12B",       "XED_REG_R13B",       "XED_REG_R14B",      "XED_REG_R15B",
    "XED_REG_AH",        "XED_REG_CH",      "XED_REG_DH",        "XED_REG_BH",         "XED_REG_ERROR",      "XED_REG_RIP",       "XED_REG_EIP",
    "XED_REG_IP",        "XED_REG_K0",      "XED_REG_K1",        "XED_REG_K2",         "XED_REG_K3",         "XED_REG_K4",        "XED_REG_K5",
    "XED_REG_K6",        "XED_REG_K7",      "XED_REG_MMX0",      "XED_REG_MMX1",       "XED_REG_MMX2",       "XED_REG_MMX3",      "XED_REG_MMX4",
    "XED_REG_MMX5",      "XED_REG_MMX6",    "XED_REG_MMX7",      "XED_REG_SSP",        "XED_REG_IA32_U_CET", "XED_REG_MXCSR",     "XED_REG_STACKPUSH",
    "XED_REG_STACKPOP",  "XED_REG_GDTR",    "XED_REG_LDTR",      "XED_REG_IDTR",       "XED_REG_TR",         "XED_REG_TSC",       "XED_REG_TSCAUX",
    "XED_REG_MSRS",      "XED_REG_FSBASE",  "XED_REG_GSBASE",    "XED_REG_TILECONFIG", "XED_REG_X87CONTROL", "XED_REG_X87STATUS", "XED_REG_X87TAG",
    "XED_REG_X87PUSH",   "XED_REG_X87POP",  "XED_REG_X87POP2",   "XED_REG_X87OPCODE",  "XED_REG_X87LASTCS",  "XED_REG_X87LASTIP", "XED_REG_X87LASTDS",
    "XED_REG_X87LASTDP", "XED_REG_ES",      "XED_REG_CS",        "XED_REG_SS",         "XED_REG_DS",         "XED_REG_FS",        "XED_REG_GS",
    "XED_REG_TMP0",      "XED_REG_TMP1",    "XED_REG_TMP2",      "XED_REG_TMP3",       "XED_REG_TMP4",       "XED_REG_TMP5",      "XED_REG_TMP6",
    "XED_REG_TMP7",      "XED_REG_TMP8",    "XED_REG_TMP9",      "XED_REG_TMP10",      "XED_REG_TMP11",      "XED_REG_TMP12",     "XED_REG_TMP13",
    "XED_REG_TMP14",     "XED_REG_TMP15",   "XED_REG_TMM0",      "XED_REG_TMM1",       "XED_REG_TMM2",       "XED_REG_TMM3",      "XED_REG_TMM4",
    "XED_REG_TMM5",      "XED_REG_TMM6",    "XED_REG_TMM7",      "XED_REG_UIF",        "XED_REG_ST0",        "XED_REG_ST1",       "XED_REG_ST2",
    "XED_REG_ST3",       "XED_REG_ST4",     "XED_REG_ST5",       "XED_REG_ST6",        "XED_REG_ST7",        "XED_REG_XCR0",      "XED_REG_XMM0",
    "XED_REG_XMM1",      "XED_REG_XMM2",    "XED_REG_XMM3",      "XED_REG_XMM4",       "XED_REG_XMM5",       "XED_REG_XMM6",      "XED_REG_XMM7",
    "XED_REG_XMM8",      "XED_REG_XMM9",    "XED_REG_XMM10",     "XED_REG_XMM11",      "XED_REG_XMM12",      "XED_REG_XMM13",     "XED_REG_XMM14",
    "XED_REG_XMM15",     "XED_REG_XMM16",   "XED_REG_XMM17",     "XED_REG_XMM18",      "XED_REG_XMM19",      "XED_REG_XMM20",     "XED_REG_XMM21",
    "XED_REG_XMM22",     "XED_REG_XMM23",   "XED_REG_XMM24",     "XED_REG_XMM25",      "XED_REG_XMM26",      "XED_REG_XMM27",     "XED_REG_XMM28",
    "XED_REG_XMM29",     "XED_REG_XMM30",   "XED_REG_XMM31",     "XED_REG_YMM0",       "XED_REG_YMM1",       "XED_REG_YMM2",      "XED_REG_YMM3",
    "XED_REG_YMM4",      "XED_REG_YMM5",    "XED_REG_YMM6",      "XED_REG_YMM7",       "XED_REG_YMM8",       "XED_REG_YMM9",      "XED_REG_YMM10",
    "XED_REG_YMM11",     "XED_REG_YMM12",   "XED_REG_YMM13",     "XED_REG_YMM14",      "XED_REG_YMM15",      "XED_REG_YMM16",     "XED_REG_YMM17",
    "XED_REG_YMM18",     "XED_REG_YMM19",   "XED_REG_YMM20",     "XED_REG_YMM21",      "XED_REG_YMM22",      "XED_REG_YMM23",     "XED_REG_YMM24",
    "XED_REG_YMM25",     "XED_REG_YMM26",   "XED_REG_YMM27",     "XED_REG_YMM28",      "XED_REG_YMM29",      "XED_REG_YMM30",     "XED_REG_YMM31",
    "XED_REG_ZMM0",      "XED_REG_ZMM1",    "XED_REG_ZMM2",      "XED_REG_ZMM3",       "XED_REG_ZMM4",       "XED_REG_ZMM5",      "XED_REG_ZMM6",
    "XED_REG_ZMM7",      "XED_REG_ZMM8",    "XED_REG_ZMM9",      "XED_REG_ZMM10",      "XED_REG_ZMM11",      "XED_REG_ZMM12",     "XED_REG_ZMM13",
    "XED_REG_ZMM14",     "XED_REG_ZMM15",   "XED_REG_ZMM16",     "XED_REG_ZMM17",      "XED_REG_ZMM18",      "XED_REG_ZMM19",     "XED_REG_ZMM20",
    "XED_REG_ZMM21",     "XED_REG_ZMM22",   "XED_REG_ZMM23",     "XED_REG_ZMM24",      "XED_REG_ZMM25",      "XED_REG_ZMM26",     "XED_REG_ZMM27",
    "XED_REG_ZMM28",     "XED_REG_ZMM29",   "XED_REG_ZMM30",     "XED_REG_ZMM31",      "XED_REG_LAST",
};

bool offset_check(uint64_t ipa, uint64_t ipb, uint8_t offset)
{
  if (ipa < ipb) {
    uint64_t tmp = ipa;
    ipa = ipb;
    ipb = tmp;
  }
  return (ipa - ipb) > offset;
}

const char* XedRegEnumToStr(uint8_t val) { return XED_REG_STR[val]; }

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

extern uint64_t trap_instrs, eret_instrs, stack_entry, stack_exits, total_instr;

template <typename T>
ooo_model_instr tracereader::read_single_instr()
{
  T trace_read_instr;

  // filter out faulty instructions (ipc1 contains a few of those)
  while (((input_instr*)&trace_read_instr)->ip == 0) {
    total_instr++;
    while (!fread(&trace_read_instr, sizeof(T), 1, trace_file)) {
      // reached end of file for this trace
      std::cout << "*** Reached end of trace: " << trace_string << std::endl; // print in any case further up

      if (knob_stop_at_completion) {
        throw EndOfTraceException("End of trace reached");
      }
      // close the trace file and re-open it
      close();
      open(trace_string);
    }
  }

  // copy the instruction into the performance model's instruction format
  ooo_model_instr retval(cpu, trace_read_instr);
  return retval;
}

void tracereader::open() { open(trace_string); }

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

/*class dynamorio_tracereader : public tracereader
{
  ooo_model_instr last_instr;
  bool initialized = false;

private:
  ooo_model_tracereader read_next_entry() {}

public:
  dynamorio_tracereader(uint8_t cpu, std::string _tn) : tracereader(cpu, _tn) {}
  ooo_model_instr get()
  {
    ooo_model_instr trace_read_instr = read_next_entry();
    if (!initialized) {
      last_instr = trace_read_instr;
      initialized = true;
      trace_read_instr = read_next_entry<dynamorio_instr>();
    }

    last_instr.branch_target = trace_read_instr.ip;
    if (knob_intel) {
      if (last_instr.is_branch
          || offset_check(last_instr.ip, trace_read_instr.ip, 15)) { // if xor is larger than 64 it must be an interrupt and thus we can't fix the size
        last_instr.size = 2;                                         // We don't know the size but it seems that the average branch instruction size is 2
      } else {
        last_instr.size = trace_read_instr.ip - last_instr.ip;
      }
    } else {
      // assume ARM trace
      last_instr.size = 4;
      if (last_instr.ip % 4 != 0) {
        assert(last_instr.ip % 2 == 0);
        last_instr.size = 2; // thumb instruction set
      }
    }
    ooo_model_instr retval = last_instr;
    assert(retval.size < 65);

    last_instr = trace_read_instr;
    return retval;
  }
};
*/

bool is_kernel(uint64_t ip) { return (bool)((ip >> 48) & 0xFFFF); }
bool is_stack(uint64_t ip) { return (bool)((ip >> 32) & 0xFFFF); }
class cloudsuite_tracereader : public tracereader
{
  ooo_model_instr last_instr;
  bool initialized = false;

public:
  cloudsuite_tracereader(uint8_t cpu, std::string _tn) : tracereader(cpu, _tn) {}

  ooo_model_instr get()
  {
    ooo_model_instr trace_read_instr = read_single_instr<cloudsuite_instr>();
    assert(trace_read_instr.ip % 4 == 0 || trace_read_instr.ip % 2 == 0 || knob_intel);

    if (!initialized) {
      last_instr = trace_read_instr;
      initialized = true;
      trace_read_instr = read_single_instr<cloudsuite_instr>();
    }

    if (is_kernel(trace_read_instr.ip) and not is_kernel(last_instr.ip)) {
      std::cout << "Kernel entry" << std::endl;
    }
    if (not is_kernel(trace_read_instr.ip) and is_kernel(last_instr.ip)) {
      std::cout << "Kernel exit" << std::endl;
    }
    if (not is_stack(trace_read_instr.ip) and is_stack(last_instr.ip)) {
      std::cout << "Stack entry" << std::endl;
    }
    if (not is_stack(trace_read_instr.ip) and is_stack(last_instr.ip)) {
      std::cout << "Stack exit" << std::endl;
    }

    last_instr.branch_target = trace_read_instr.ip;
    if (knob_intel) {
      if (last_instr.is_branch
          || offset_check(last_instr.ip, trace_read_instr.ip, 15)) { // if xor is larger than 64 it must be an interrupt and thus we can't fix the size
        last_instr.size = 2;                                         // We don't know the size but it seems that the average branch instruction size is 2
      } else {
        last_instr.size = trace_read_instr.ip - last_instr.ip;
      }
    } else {
      // assume ARM trace
      last_instr.size = 4;
      if (last_instr.ip % 4 != 0) {
        assert(last_instr.ip % 2 == 0);
        last_instr.size = 2; // thumb instruction set
      }
    }

    ooo_model_instr retval = last_instr;
    assert(retval.size < 65);

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

    assert(trace_read_instr.ip % 4 == 0 || trace_read_instr.ip % 2 == 0 || knob_intel);

    if (!initialized) {
      last_instr = trace_read_instr;
      initialized = true;
      trace_read_instr = read_single_instr<input_instr>();
    }

    last_instr.branch_target = trace_read_instr.ip;

    // adjust to test context crossing speedup improvements
    if (not is_stack(trace_read_instr.ip) and is_stack(last_instr.ip)) {
      stack_entry++;
    }
    if (is_kernel(trace_read_instr.ip) and not is_kernel(last_instr.ip)) {
      trap_instrs++;
    }
    if (not is_kernel(trace_read_instr.ip) and is_kernel(last_instr.ip)) {
      eret_instrs++;
    }
    if (not is_stack(last_instr.ip) and is_stack(trace_read_instr.ip)) {
      stack_exits++;
    }

    if (knob_intel) {
      if (last_instr.is_branch
          || offset_check(last_instr.ip, trace_read_instr.ip, 15)) { // if xor is larger than  it must be an interrupt and thus we can't fix the size
        last_instr.size = 2;                                         // We don't know the size but it seems that the average branch instruction size is 2
      } else {
        last_instr.size = trace_read_instr.ip - last_instr.ip;
      }
    } else {
      // assume ARM trace
      last_instr.size = 4;
      if (last_instr.ip % 4 != 0) {
        assert(last_instr.ip % 2 == 0);
        last_instr.size = 2; // thumb instruction set
      }
    }
    ooo_model_instr retval = last_instr;
    assert(retval.size < 65);

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

  // Inspired by the makeNop function in thermometer version of ChampSim
  xed_decoded_inst_t make_nop(uint64_t size)
  {
    xed_decoded_inst_t ins;
    xed_decoded_inst_zero(&ins);
    xed_decoded_inst_set_mode(&ins, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b);
    uint8_t buf[16];
    xed_error_enum_t res = xed_encode_nop(&buf[0], size);
    if (res != XED_ERROR_NONE) {
      std::cerr << "XED NOP encoder error: " << xed_error_enum_t2str(res);
      assert(0);
    }
    res = xed_decode(&ins, buf, size);
    if (res != XED_ERROR_NONE) {
      std::cerr << "XED NOP decode error: " << xed_error_enum_t2str(res);
      assert(0);
    }
    return ins;
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
    char buffer[GZ_BUFFER_SIZE] = {0};
    pt_instr trace_read_instr_pt;
    do {

      std::memset(buffer, 0, GZ_BUFFER_SIZE);
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
    total_decoding_instructions++; // should correspond to simulated instructions (at least)
    if (xed_error != XED_ERROR_NONE) {
      // std::cerr << "DECODING OF PT INSTR FAILED: " << xed_error << std::endl;
      raw_pt_inst = make_nop(trace_read_instr_pt.size);
      failed_decoding_instructions++;
      // assert(0);
    }

    trace_read_instr_pt.decoded_instruction = raw_pt_inst;

    uint32_t numOperands = xed_decoded_inst_noperands(&raw_pt_inst);
    uint32_t inRegIdx = 0, outRegIdx = 0;
    uint32_t inMemIdx = 0, outMemIdx = 0;
    for (uint32_t opIdx = 0; opIdx < numOperands; opIdx++) {
      bool readOp = false, writeOp = false;
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
        auto reg = xed_decoded_inst_get_reg(&raw_pt_inst, opName);
        reg = xed_get_largest_enclosing_register(reg);

        if (readOp) {
          trace_read_instr_pt.source_registers[inRegIdx++] = reg;
        }
        if (writeOp) {
          trace_read_instr_pt.destination_registers[outRegIdx++] = reg;
        }

        // We can at most have six registers:
        // divide RDX:RAX / Rx (R), result in RAX/RDX (R/W) and RFLAGS(W)
        // new record: CMPXCHG16B RDx, RAX (R/W), RCX, RBX (R) and RFLAGS (W)
        if (inRegIdx + outRegIdx > 7) {
          char buf[2048];
          xed_decoded_inst_dump(&raw_pt_inst, buf, 2048);
          std::cout << "Processing " << buf << std::endl;
          for (uint32_t i = 0; i < inRegIdx; i++) {
            std::cout << "inreg " << i << ": " << XedRegEnumToStr(trace_read_instr_pt.source_registers[i]) << std::endl;
          }
          for (uint32_t i = 0; i < outRegIdx; i++) {
            std::cout << "outreg " << i << ": " << XedRegEnumToStr(trace_read_instr_pt.destination_registers[i]) << std::endl;
          }
          assert(0);
        }
      } else if (opName == XED_OPERAND_MEM0 || opName == XED_OPERAND_MEM1) {
        // TODO: we don-t have memory in those traces / might need to trace on our own
        // char buf[2048];
        // xed_decoded_inst_dump(&raw_pt_inst, buf, 2048);
        // printf("0x");
        // for (int bi = 0; bi < trace_read_instr_pt.size; bi++)
        //   printf("%02X", raw_pt_inst._byte_array._enc[bi]);
        // printf("\n");
        // std::cout << "Processing " << buf << std::endl;
        trace_read_instr_pt.mem_refs += 1;
        if (readOp) {
          trace_read_instr_pt.source_memory[inMemIdx++] = 1;
        }
        if (writeOp) {
          trace_read_instr_pt.destination_memory[outMemIdx++] = 1;
        }

        // We just count the number of accesses we don't care about the access itself for now.
        // will overexagerate the effect on ipc, but effect can still be provable
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
