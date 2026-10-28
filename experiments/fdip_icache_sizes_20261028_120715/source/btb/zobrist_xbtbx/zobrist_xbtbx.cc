#include "zobrist_xbtbx.h"
zobrist_xbtbx::zobrist_xbtbx(O3_CPU* cpu) : litebtb_predictor(cpu, litebtb_hash::zobrist) {}
