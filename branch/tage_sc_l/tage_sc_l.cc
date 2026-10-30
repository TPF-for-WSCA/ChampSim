#include "tage_sc_l.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "instruction.h"

using std::abs;
using std::pow;

#include "tage_sc_l.inc"

tage_sc_l::tage_sc_l(O3_CPU* cpu) : branch_predictor(cpu), predictor(std::make_unique<state>()) {}
tage_sc_l::~tage_sc_l() = default;
tage_sc_l::tage_sc_l(tage_sc_l&&) noexcept = default;
tage_sc_l& tage_sc_l::operator=(tage_sc_l&&) noexcept = default;

bool tage_sc_l::predict_branch(champsim::address ip) { return predictor->GetPrediction(ip.to<uint64_t>()); }

void tage_sc_l::last_branch_result(champsim::address ip, champsim::address target, bool taken, uint8_t branch_type)
{
  if (branch_type == BRANCH_CONDITIONAL) {
    // Prediction scratch may have been overwritten by another lookup (including
    // wrong-path fetch). Histories and tables change only in this callback.
    const bool prediction = predictor->GetPrediction(ip.to<uint64_t>());
    predictor->UpdatePredictor(ip.to<uint64_t>(), branch_type, taken, prediction, target.to<uint64_t>());
  } else {
    // Match Jimenez's adapter: other branches update history without training
    // direction tables. In particular, do not train on wrong-path lookups.
    predictor->TrackOtherInst(ip.to<uint64_t>(), branch_type, taken, target.to<uint64_t>());
  }
}
