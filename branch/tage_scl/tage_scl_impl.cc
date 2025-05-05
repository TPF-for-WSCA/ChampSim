
#include "ooo_cpu.h"
#include "tage_scl.h"

LLBP::TSCLConfig config;
LLBP::TageSCL predictor(config);

void O3_CPU::initialize_branch_predictor() {}

uint8_t O3_CPU::predict_branch(uint64_t ip) {
  return predictor.GetPrediction(ip);
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type) {
  predictor.UpdatePredictor(ip, taken, predictor.GetPrediction(ip), branch_target);
}


