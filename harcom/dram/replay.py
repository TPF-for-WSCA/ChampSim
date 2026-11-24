#!/usr/bin/env python3
"""Replay measured ChampSim DRAM bursts through DRAMPower v6.0.0."""
import argparse
from contextlib import nullcontext
from collections import Counter
import csv
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory


@dataclass(frozen=True)
class Timing:
    tick_ps: float
    activate_guard: int
    row_guard: int
    precharge: int
    refresh: int
    interval: int
    read_latency: int
    write_latency: int
    burst_guard: int
    ranks: int
    payload_bytes: int

    @classmethod
    def from_memspec(cls, memspec, geometry):
        device = memspec["memoryType"]
        if device not in ("DDR4", "DDR5"):
            raise ValueError("replay supports only DDR4 and DDR5")
        arch, t = memspec["memarchitecturespec"], memspec["memtimingspec"]
        if arch["RefMode"] != 1 or t.get("AL", 0):
            raise ValueError("replay requires 1x all-bank refresh and zero additive latency")
        groups = arch["nbrOfBankGroups"]
        if groups <= 0 or arch["nbrOfBanks"] % groups or geometry["channels"] <= 0:
            raise ValueError("invalid channel or bank-group geometry")
        expected = {
            "ranks": arch["nbrOfRanks"],
            "bankgroups": groups,
            "banks_per_group": arch["nbrOfBanks"] // groups,
            "rows": arch["nbrOfRows"],
            "columns": arch["nbrOfColumns"],
            "channel_width_bits": arch["width"] * arch["nbrOfDevices"],
            "burst_length": arch["burstLength"],
        }
        for field, value in expected.items():
            if geometry[field] != value:
                raise ValueError(f"memspec {field}={value} differs from ChampSim {geometry[field]}")
        tick_ps = t["tCK"] * 1e12
        if not math.isfinite(tick_ps) or tick_ps <= 0 or arch["dataRate"] != 2:
            raise ValueError("replay requires a positive device clock and double data rate")
        # ChampSim truncates configured periods to integer picoseconds.
        if abs(tick_ps - geometry["clock_ps"]) >= 1 or abs(tick_ps / 2 - geometry["beat_ps"]) >= 1:
            raise ValueError("memspec command or data clock differs from ChampSim")
        # Guard across all banks and ranks, including same-group and multi-rank
        # constraints. Serializing ACTs by FAW is conservative.
        activate = max(2, t["RCD"], *(v for k, v in t.items() if k.startswith(("RRD", "FAW"))))
        row = max(t["RAS"], t.get("RC", 0), t["RAS"] + t["RP"])
        precharge = max(2, t["RP"], t.get("PPD", 0))
        refresh = max(t["RFC1"], t.get("RFC1_slr", 0), t.get("RFC1_dlr", 0), t.get("RFC1_dpr", 0))
        interval = math.floor(geometry["refresh_interval_ps"] / tick_ps)
        device_interval = t.get("REFI1" if device == "DDR5" else "REFI", 0)
        if device_interval:
            interval = min(interval, device_interval)
        # Wait for either direction's worst data and recovery time before
        # allowing another command. This covers data overlap and turnarounds.
        burst = math.ceil(arch["burstLength"] / arch["dataRate"])
        burst_guard = max(t["RL"], t["WL"]) + burst + max(
            2, t["RTP"], t["WR"], *(v for k, v in t.items() if k.startswith(("CCD", "WTR", "RTRS", "RDQS")))
        ) + max(t.get("RPRE", 0), t.get("WPRE", 0), t.get("RPST", 0), t.get("WPST", 0))
        return cls(tick_ps, activate, row, precharge, refresh, interval, t["RL"], t["WL"], burst_guard,
                   expected["ranks"], expected["channel_width_bits"] * expected["burst_length"] // 8)


class Replay:
    def __init__(self, timing):
        self.t = timing
        self.commands = []
        self.open_rows = {}
        self.cursor = 0
        self.refresh_window = timing.ranks * (timing.precharge + timing.refresh)
        self.deadline = timing.interval
        self.transaction_bound = timing.precharge + timing.activate_guard + timing.row_guard + timing.burst_guard
        if timing.refresh <= 0 or self.refresh_window + self.transaction_bound >= timing.interval:
            raise ValueError("refresh interval cannot accommodate the serialized replay")

    def emit(self, command, rank=0, group=0, bank=0, row=0, column=0):
        self.commands.append((self.cursor, command, rank, group, bank, row, column))

    def refresh(self):
        self.cursor = max(self.cursor, self.deadline - self.refresh_window)
        for rank in range(self.t.ranks):
            if any(key[0] == rank for key in self.open_rows):
                self.emit("PREA", rank)
                self.open_rows = {key: row for key, row in self.open_rows.items() if key[0] != rank}
            # Keep rank refreshes periodic even when a rank was already closed.
            self.cursor += self.t.precharge
            self.emit("REFA", rank)
            self.cursor += self.t.refresh
        if self.cursor > self.deadline:
            raise ValueError("replay missed a refresh deadline")
        self.deadline += self.t.interval

    def burst(self, arrival_ps, write, rank, group, bank, row, column):
        latency = self.t.write_latency if write else self.t.read_latency
        ready = max(0, math.ceil(arrival_ps / self.t.tick_ps) - latency)
        while max(self.cursor, ready) + self.transaction_bound > self.deadline - self.refresh_window:
            self.refresh()
        self.cursor = max(self.cursor, ready)
        key = (rank, group, bank)
        activate_time = None
        if key in self.open_rows and self.open_rows[key] != row:
            self.emit("PRE", *key)
            self.cursor += self.t.precharge
            del self.open_rows[key]
        if key not in self.open_rows:
            activate_time = self.cursor
            self.emit("ACT", *key, row)
            self.cursor += self.t.activate_guard
            self.open_rows[key] = row
        self.emit("WR" if write else "RD", *key, row, column)
        self.cursor += self.t.burst_guard
        if activate_time is not None:
            self.cursor = max(self.cursor, activate_time + self.t.row_guard)

    def finish(self, horizon):
        while self.deadline <= horizon:
            self.refresh()
        self.cursor = horizon
        self.emit("END")

    def write_trace(self, path):
        # The CLI requires a data column for RD/WR even in toggle-rate mode.
        placeholder = "00" * self.t.payload_bytes
        with path.open("w", newline="") as out:
            writer = csv.writer(out)
            for entry in self.commands:
                writer.writerow((*entry, placeholder) if entry[1] in ("RD", "WR") else entry)


def read_events(path):
    with (nullcontext(sys.stdin) if path == "-" else Path(path).open()) as source:
        geometry = json.loads(source.readline())
        if geometry.get("format") != "champsim-dram-events-v1":
            raise ValueError("unsupported ChampSim DRAM event format")
        streams = [[] for _ in range(geometry["channels"])]
        native = [Counter() for _ in streams]
        begin = end = None
        for entry in csv.DictReader(source):
            kind, time = entry["event"], int(entry["time_ps"])
            if kind == "begin":
                if begin is not None or end is not None:
                    raise ValueError("expected one measured phase")
                begin = time
            elif kind == "end":
                if begin is None or end is not None or time <= begin:
                    raise ValueError("invalid measured phase boundary")
                end = time
            else:
                if begin is None or end is not None:
                    raise ValueError("event outside measured phase")
                if kind not in {"read", "write", "refresh_due", "refresh_start", "refresh_end",
                                "reserve_hit", "reserve_conflict", "reserve_closed", "cancel_close", "cancel_keep"}:
                    raise ValueError(f"unknown native DRAM event: {kind}")
                channel = int(entry["channel"])
                if not 0 <= channel < len(streams):
                    raise ValueError("invalid channel number")
                native[channel][kind] += 1
                if kind in ("read", "write", "refresh_due"):
                    target = tuple(int(entry[field]) for field in ("rank", "bankgroup", "bank", "row", "column"))
                    streams[channel].append((time, kind, target))
        if begin is None or end is None:
            raise ValueError("missing measured phase boundary")
    return geometry, begin, end, streams, native


def run(args, output):
    geometry, begin, end, streams, native = read_events(args.events)
    memspec = json.loads(args.memspec.read_text())["memspec"]
    timing = Timing.from_memspec(memspec, geometry)
    replays = []
    outside = []
    for stream in streams:
        replay = Replay(timing)
        skipped = 0
        for time, kind, target in sorted(stream, key=lambda item: item[0]):
            if kind == "refresh_due":
                continue  # Replay regenerates the matching periodic refreshes on the legal clock.
            if not begin <= time < end:
                skipped += 1
                continue
            fields = ("ranks", "bankgroups", "banks_per_group", "rows", "columns")
            if any(not 0 <= index < geometry[field] for index, field in zip(target, fields)):
                raise ValueError(f"target outside configured geometry: {target}")
            if target[-1] % geometry["burst_length"]:
                raise ValueError(f"unaligned burst column: {target[-1]}")
            replay.burst(time - begin, kind == "write", *target)
        replays.append(replay)
        outside.append(skipped)
    horizon = max(math.ceil((end - begin) / timing.tick_ps), *(r.cursor for r in replays))
    for replay in replays:
        replay.finish(horizon)

    config = output / "toggles.json"
    config.write_text(json.dumps({"useToggleRate": True, "simconfig": {"toggleRateDefinition": {
        "togglingRateRead": args.toggle, "togglingRateWrite": args.toggle,
        "dutyCycleRead": args.toggle, "dutyCycleWrite": args.toggle,
        "idlePatternRead": "L", "idlePatternWrite": "L",
    }}}))
    results = []
    energy_total = 0.0
    for channel, replay in enumerate(replays):
        trace = output / f"channel-{channel}.csv"
        replay.write_trace(trace)
        entry = {"channel": channel, "native_events": native[channel], "bursts_outside_phase": outside[channel],
                 "replay_commands": Counter(cmd[1] for cmd in replay.commands)}
        if args.drampower:
            result = output / f"channel-{channel}.json"
            result.touch()
            subprocess.run([str(args.drampower.resolve()), "-c", str(config), "-m", str(args.memspec),
                            "-t", str(trace), "-j", str(result)], check=True, stdout=sys.stderr)
            energy = json.loads(result.read_text())["TotalEnergy"]
            if not math.isfinite(energy) or energy < 0:
                raise ValueError("DRAMPower produced invalid energy")
            entry["drampower_energy_joules"] = energy
            energy_total += energy
        results.append(entry)
    duration = horizon * timing.tick_ps
    summary = {
        "coverage": "timing-checked serialized replay",
        "native_device_energy_incomplete": True,
        "drampower_cli_format": "6.0.0",
        "memspec_sha256": hashlib.sha256(args.memspec.read_bytes()).hexdigest(),
        "native_duration_ps": end - begin,
        "replay_duration_ps": duration,
        "replay_refresh_interval_cycles": timing.interval,
        "dq_toggle_rate_and_duty_cycle": args.toggle,
        "channels": results,
    }
    if args.drampower:
        summary.update(drampower_cli_sha256=hashlib.sha256(args.drampower.read_bytes()).hexdigest(),
                       replay_energy_joules=energy_total, replay_average_power_watts=energy_total / (duration * 1e-12))
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--events", required=True, help="native event file or - for stdin")
    parser.add_argument("--memspec", required=True, type=Path)
    output = parser.add_mutually_exclusive_group(required=True)
    output.add_argument("--output", type=Path, help="new output directory")
    output.add_argument("--summary-stdout", action="store_true", help="use temporary command files and emit only the summary")
    parser.add_argument("--drampower", type=Path, help="path to upstream v6.0.0 CLI. Omit to generate traces only.")
    parser.add_argument("--toggle", type=float, default=0.5)
    args = parser.parse_args()
    if not 0 <= args.toggle <= 1:
        parser.error("--toggle must be between 0 and 1")
    try:
        if args.summary_stdout:
            with TemporaryDirectory(prefix="champsim-drampower-") as temporary:
                print(json.dumps(run(args, Path(temporary))))
        else:
            args.output.mkdir()
            summary = run(args, args.output)
            (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    except (KeyError, ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
