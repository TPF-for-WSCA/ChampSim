from io import StringIO
import unittest
from unittest.mock import patch

from harcom.dram.replay import Replay, Timing, read_events


class DramReplayTests(unittest.TestCase):
    def setUp(self):
        self.timing = Timing(625, 24, 72, 24, 112, 1000, 16, 12, 45, 2, 64)

    def test_open_row_hit_conflict_and_turnaround(self):
        replay = Replay(self.timing)
        for when, write, row in [(0, False, 1), (0, True, 1), (0, False, 2)]:
            replay.burst(when, write, 0, 0, 0, row, 0)
        commands = replay.commands
        self.assertEqual([c[1] for c in commands], ["ACT", "RD", "WR", "PRE", "ACT", "RD"])
        act, read, write, pre, next_act, next_read = [c[0] for c in commands]
        self.assertGreaterEqual(read - act, 24)
        self.assertGreaterEqual(write - read, 45)
        self.assertGreaterEqual(pre - write, 45)
        self.assertGreaterEqual(pre - act, 72)
        self.assertGreaterEqual(next_act - pre, 24)
        self.assertGreaterEqual(next_read - next_act, 24)

    def test_refresh_reopens_rows_and_preserves_idle_time(self):
        replay = Replay(self.timing)
        replay.burst(0, False, 0, 0, 0, 1, 0)
        replay.burst(1000 * 625, False, 0, 0, 0, 1, 0)
        replay.finish(3000)
        self.assertEqual([c[1] for c in replay.commands].count("ACT"), 2)
        self.assertIn("PREA", [c[1] for c in replay.commands])
        for rank in (0, 1):
            refresh = [cmd[0] for cmd in replay.commands if cmd[1] == "REFA" and cmd[2] == rank]
            self.assertEqual(len(refresh), 3)
            self.assertEqual([b - a for a, b in zip(refresh, refresh[1:])], [1000, 1000])
        self.assertEqual(replay.commands[-1][:2], (3000, "END"))

    def test_native_event_stream_can_be_read_from_stdin(self):
        source = StringIO(
            '{"format":"champsim-dram-events-v1","channels":1}\n'
            'event,time_ps,channel,rank,bankgroup,bank,row,column\n'
            'begin,0,0,0,0,0,0,0\n'
            'read,10,0,0,0,0,1,0\n'
            'end,20,0,0,0,0,0,0\n'
        )
        with patch("sys.stdin", source):
            _, begin, end, streams, native = read_events("-")
        self.assertEqual((begin, end), (0, 20))
        self.assertEqual(streams, [[(10, "read", (0, 0, 0, 1, 0))]])
        self.assertEqual(native[0]["read"], 1)

    def test_impossible_refresh_window_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "refresh interval"):
            Replay(Timing(625, 24, 72, 24, 112, 100, 16, 12, 45, 2, 64))


if __name__ == "__main__":
    unittest.main()
