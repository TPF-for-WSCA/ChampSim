#ifndef BTB_ZOBRIST_XBTBX_H
#define BTB_ZOBRIST_XBTBX_H
#include "litebtb.h"
class zobrist_xbtbx : public litebtb_predictor { public: explicit zobrist_xbtbx(O3_CPU* cpu); };
#endif
