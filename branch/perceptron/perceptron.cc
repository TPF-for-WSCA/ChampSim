/*
 * Copyright (c) 2001 University of Texas at Austin
 *
 * Daniel A. Jimenez
 * Calvin Lin
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software (the "Software"), to deal in
 * the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE UNIVERSITY OF TEXAS AT
 * AUSTIN BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file implements the simulated perceptron branch predictor from:
 *
 * Jimenez, D. A. & Lin, C., Dynamic branch prediction with perceptrons,
 * Proceedings of the Seventh International Symposium on High Performance
 * Computer Architecture (HPCA), Monterrey, NL, Mexico 2001
 *
 * The #define's here specify a perceptron predictor with a history
 * length of 24, 163 perceptrons, and  8-bit weights.  This represents
 * a hardware budget of (24+1)*8*163 = 32600 bits, or about 4K bytes,
 * which is comparable to the hardware budget of the Alpha 21264 hybrid
 * branch predictor.
 */

#include "perceptron.h"

#include <cmath>

bool perceptron::predict_branch(champsim::address ip)
{
  // hash the address to get an index into the table of perceptrons
  const auto index = ip.to<uint64_t>() % NUM_PERCEPTRONS;
  return perceptrons[index].predict(global_history) >= 0;
}

void perceptron::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  // Recompute from the real history: intervening lookups have no outcomes and
  // must neither supply training context nor evict a real branch's update.
  const auto index = ip.to<uint64_t>() % NUM_PERCEPTRONS;
  const auto output = perceptrons[index].predict(global_history);
  const bool prediction = output >= 0;

  // if the output of the perceptron predictor is outside of the range
  // [-THETA,THETA] *and* the prediction was correct, then we don't need to
  // adjust the weights
  const auto THETA = std::lround(1.93 * PERCEPTRON_HISTORY + 14); // threshold for training
  if ((output <= THETA && output >= -THETA) || (prediction != taken)) {
    perceptrons[index].update(taken, global_history);
  }

  global_history <<= 1;
  global_history.set(0, taken);
}
