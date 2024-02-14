#include <cstdio>
#include <stdexcept>
#include <string>

#include "instruction.h"

class EndOfTraceException : public std::runtime_error
{
  using std::runtime_error::runtime_error;
};

class tracereader
{
protected:
  FILE* trace_file = NULL;
  uint8_t cpu;
  std::string cmd_fmtstr;
  std::string decomp_program;
  std::string trace_string;

public:
  uint64_t failed_decoding_instructions = 0;
  uint64_t total_decoding_instructions = 0;
  tracereader(const tracereader& other) = delete;
  tracereader(uint8_t cpu, std::string _ts);
  ~tracereader();
  void open(std::string trace_string);
  void open();
  void close();

  template <typename T>
  ooo_model_instr read_single_instr();

  virtual ooo_model_instr get() = 0;
};

tracereader* get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite, bool is_pt = false);
