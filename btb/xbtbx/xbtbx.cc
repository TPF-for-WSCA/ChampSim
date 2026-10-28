#include "xbtbx.h"
xbtbx::xbtbx(O3_CPU* cpu) : litebtb_predictor(cpu, litebtb_hash::identity) {}
