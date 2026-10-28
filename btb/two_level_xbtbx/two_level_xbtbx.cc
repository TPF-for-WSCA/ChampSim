#include "two_level_xbtbx.h"
two_level_xbtbx::two_level_xbtbx(O3_CPU* cpu) : litebtb_predictor(cpu, litebtb_hash::identity, true) {}
