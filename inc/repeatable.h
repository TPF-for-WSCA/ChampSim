/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef REPEATABLE_H
#define REPEATABLE_H

#include <memory>
#include <string>

#include "instruction.h"
#include <fmt/ranges.h>

namespace champsim
{
template <typename T, typename... Args>
struct repeatable {
  static_assert(std::is_move_constructible_v<T>);
  static_assert(std::is_move_assignable_v<T>);
  std::tuple<Args...> args_;
  T intern_{std::apply([](auto... x) { return T{x...}; }, args_)};
  explicit repeatable(Args... args) : args_(args...) {}
  unsigned long long inst_count = 0;

  auto operator()()
  {
    // Reopen trace if we've reached the end of the file
    if (intern_.eof()) {
      fmt::print("*** Reached end of trace: {}\n", args_);
      fmt::print("|After {} instructions|\n", inst_count);
      intern_ = T{std::apply([](auto... x) { return T{x...}; }, args_)};
    }
    inst_count++;

    return intern_();
  }

  bool eof() const { return false; }
};
} // namespace champsim

#endif
