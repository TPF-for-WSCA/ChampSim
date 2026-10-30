#include <catch.hpp>

#include "instr.h"
#include "mocks.hpp"
#include "ooo_cpu.h"

SCENARIO("The fetch bandwidth limits the number of packets issued each cycle")
{
  auto bandwidth = GENERATE(as<long>{}, 1, 2, 3, 4, 5);
  GIVEN("An instruction buffer with many different instructions")
  {
    constexpr std::array<uint64_t, 6> addrs{{0xdeadbeef, 0xbeefdead, 0xcafebabe, 0xbabecafe, 0xfeeddeaf, 0xdeaffeed}};

    do_nothing_MRC mock_L1I;
    do_nothing_MRC mock_L1D;
    O3_CPU uut{
        champsim::core_builder{}.fetch_queues(&mock_L1I.queues).data_queues(&mock_L1D.queues).fetch_width(champsim::bandwidth::maximum_type{6}).l1i_bandwidth(champsim::bandwidth::maximum_type{bandwidth})};

    std::array<champsim::operable*, 3> elements = {&uut, &mock_L1I, &mock_L1D};

    std::for_each(std::begin(addrs), std::end(addrs), [&](auto x) {
      uut.IFETCH_BUFFER.push_back(champsim::test::instruction_with_ip(x));
      uut.IFETCH_BUFFER.back().dib_checked = true;
    });

    WHEN("The fetch operates")
    {
      for (auto x : elements)
        x->_operate();

      THEN("The bandwidth number of packets are issued") { REQUIRE(mock_L1I.packet_count() == (long unsigned)bandwidth); }
    }
  }
}

TEST_CASE("Fetch width bounds instructions across all cache requests in a cycle", "[fetch-width]")
{
  const bool same_line = GENERATE(false, true);
  champsim::channel requests{32, 32, 32, champsim::data::bits{6}, false};
  O3_CPU cpu{champsim::core_builder{}.fetch_queues(&requests).fetch_width(champsim::bandwidth::maximum_type{3})
                 .l1i_bandwidth(champsim::bandwidth::maximum_type{8})};
  for (unsigned i = 0; i < 8; ++i) {
    auto instr = champsim::test::instruction_with_ip(0x1000 + i * (same_line ? 4 : 64));
    instr.dib_checked = true;
    cpu.IFETCH_BUFFER.push_back(instr);
  }
  cpu.fetch_instruction();
  REQUIRE(std::count_if(cpu.IFETCH_BUFFER.begin(), cpu.IFETCH_BUFFER.end(), [](auto& i) { return i.fetch_issued; }) == 3);
  REQUIRE(requests.RQ.size() == (same_line ? 1 : 3));
  for (auto& request : requests.RQ)
    REQUIRE(request.instr_depend_on_me.size() <= 3);
  cpu.fetch_instruction();
  REQUIRE(std::count_if(cpu.IFETCH_BUFFER.begin(), cpu.IFETCH_BUFFER.end(), [](auto& i) { return i.fetch_issued; }) == 6);
}
