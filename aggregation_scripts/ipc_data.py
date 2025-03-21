#!/usr/bin/python
import os
import re
import sys
from collections import defaultdict
from enum import Enum


class STATS(Enum):
    IPC = 1
    MPKI = 2
    PARTIAL = 3
    BUFFER_DURATION = 4
    USELESS = 5
    FRONTEND_STALLS = 6
    PARTIAL_MISSES = 7
    BRANCH_DISTANCES = 8
    BRANCH_COUNT = 9
    INSTRUCTION_COUNT = 10
    BRANCH_MPKI = 11
    FETCH_COUNT = 12
    STALL_CYCLES = 13
    ROB_AT_MISS = 14
    BIG_TARGET_COUNT = 15
    CONTEXT_SWITCH = 16
    NUM_BTB_BITS_PER_CL = 17
    ALIASING = 18
    BIT_INFORMATION = 19
    BTB_TAG_ENTROPY = 20
    BTB_BIT_ORDERING = 21
    BTB_BIT_ORDERING_SWITCHED = 22
    ABSOLUTE_ALIASING = 23
    REGION_SPLIT = 22


type = STATS.IPC

buffer = False


def extract_ipc(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    regex = re.compile(r"CPU 0 cumulative IPC: (\d*\.?\d+)")
    logs.reverse()
    for line in logs:  # reverse to find last run first
        matches = regex.match(line)
        if matches:
            return matches.groups()[0]


def extract_branch_count(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    regex = re.compile(r"Branch count: (\d+)")
    logs.reverse()
    for line in logs:
        matches = regex.match(line)
        if matches:
            return int(matches.groups()[0])


def extract_big_offset_counts(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    regex = re.compile(r"XXX Big offset targets total: (\d+)")
    logs.reverse()
    for line in logs:
        matches = regex.match(line)
        if matches:
            return int(matches.groups()[0])


def extract_instruction_count(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    regex = re.compile(r"CPU \d+ cumulative IPC: \d+.\d+ instructions: (\d+)")
    logs.reverse()
    for line in logs:
        matches = regex.match(line)
        if matches:
            return int(matches.groups()[0])


def extract_btb_tag_entropy(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    entropy_re = re.compile(r"BTB TAG Entropy: ([0-9]*(\.[0-9]+)?)")
    for line in logs:
        matches = entropy_re.search(line)
        if matches:
            return float(matches.groups()[0])

def extract_btb_region_split(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    logs.reverse()
    region_split_re = re.compile(r"CPU 0 REGION BTB BIG REGIONS: (\d+)")
    for line in logs:
        matches = region_split_re.search(line)
        if matches:
            return int(matches.groups()[0])

def extract_btb_bit_ordering(path):
    import itertools

    logs = []
    with open(path) as f:
        logs = f.readlines()
    entropy_re = re.compile(r"BTB TAG Switched Bit IDX\tENTROPY")
    ilogs = iter(logs)
    for line in ilogs:
        matches = entropy_re.search(line)
        if matches:
            break
    bit_ordering = []
    for line in ilogs:
        if (
            line.startswith("BTB")
            or line.startswith("Branch")
            or line.startswith("BRANCH")
        ):
            break
        bit_ordering.append(int(line.split("\t")[0].strip()))
    return bit_ordering


def extract_bit_information(path):
    import itertools

    logs = []
    with open(path) as f:
        logs = f.readlines()
    title_re = re.compile(r"XXX Total 1 bits in branch IPs:")
    ilogs = iter(logs)
    for line in ilogs:
        if title_re.match(line):
            break
    bit_data = []
    for line in ilogs:
        if line.startswith("cpu0") or line.startswith("XXX"):
            break
        bit_data.append(float(line.split(":")[1].strip()))
    # assert(len(bit_data) == 64)
    # if not successful we have empty list - make none list
    if len(bit_data) == 0:
        bit_data = [None for _ in range(64)]
    if len(bit_data) < 64:
        assert 0
    return bit_data


def extract_absolute_btb_aliasing(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    logs.reverse()
    # order of values: total, aliasing, same block, different block
    re_total = re.compile(r"Total Aliasing: (\d+)")
    total = 0
    for line in logs:
        matches = re_total.search(line)
        if matches:
            total = int(matches.groups()[0])
            break
    return total


def extract_positive_btb_aliasing(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    logs.reverse()
    # order of values: total, aliasing, same block, different block
    re_list = [
        re.compile(r"Negative Aliasing: (\d+)"),
        re.compile(r"Total Aliasing: (\d+)"),
    ]
    lookups = [0, 0]
    for line in logs:
        for idx, reg in enumerate(re_list):
            matches = reg.search(line)
            if matches:
                lookups[idx] = int(matches.groups()[0])
                break
        if all(lookups):
            break
    return 0 if not lookups[1] else lookups[0] / lookups[1]


def extract_btb_bits_per_cl(path):
    import itertools

    logs = []
    with open(path) as f:
        logs = f.readlines()
    ilogs = iter(logs)
    for line in ilogs:
        if line.strip() == "XXX num_cachelines_required_n_bits":
            break
    data = {}
    max_bits = 0
    for data_line in ilogs:
        if data_line.startswith("XXX"):
            break
        [num_bits, num_cl] = [int(val.strip()) for val in data_line.split(":")]
        data[num_bits] = num_cl
        max_bits = num_bits

    no_holes = [data.get(num_bits, 0) for num_bits in range(max_bits + 1)]
    total = sum(no_holes)
    if total == 0:
        return []
    no_holes = list(itertools.accumulate(no_holes))
    no_holes = [elem / total for elem in no_holes]

    return no_holes


def extract_context_switch_count(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    logs.reverse()
    ilogs = iter(logs)
    for line in ilogs:
        if line == "XXX num_cachelines_required_n_bits":
            break
    data = {}
    max_bits = 0
    for data_line in ilogs:
        if data_line.startswith("XXX"):
            break
        [num_bits, num_cl] = [int(val.strip()) for val in data_line.split(":")]
        data[num_bits] = num_cl
        max_bits = num_bits

    no_holes = [data.get(num_bits, 0) for num_bits in range(max_bits)]

    kernel_enters = re.compile()


def extract_l1i_partial(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    regex = re.compile(r"cpu0\_L1I TOTAL.*PARTIAL MISS:\s+(\d*) \( (\d*\.?\d+)\%\)")
    logs.reverse()
    for line in logs:  # reverse to find last run first
        matches = regex.match(line)
        if matches:
            return matches.groups()[1]
    return 0


def extract_fetches(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    regex = re.compile(r"cpu0\_L1I LOAD.*ACCESS:\s+(\d*)")
    logs.reverse()
    for line in logs:  # reverse to find last run first
        matches = regex.match(line)
        if matches:
            return int(matches.groups()[0])
    return 0


def extract_l1i_detail_partial_misses(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    merges_regex = re.compile(
        "cpu0_L1I_buffer PARTIAL MISSES\tUNDERRUNS:\s+(\d+)\tOVERRUNS:\s+(\d+)\tMERGES:\s+(\d+)\tNEW BLOCKS:\s+(\d+)"
    )

    logs.reverse()
    merges = (0,)
    for line in logs:
        merges = merges_regex.match(line)
        if merges:
            merges = (
                int(merges.groups()[0]),
                int(merges.groups()[1]),
                int(merges.groups()[2]),
                int(merges.groups()[3]),
            )
            break
    return merges


def extrace_useless_percentage(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    regex = re.compile(r"cpu0\_L1I EVICTIONS:\s+(\d*)\s+USELESS:\s+(\d*)")
    if buffer:
        regex = re.compile(r"cpu0\_L1I_buffer EVICTIONS:\s+(\d*)\s+USELESS:\s+(\d*)")

    logs.reverse()
    for line in logs:  # reverse to find last run first
        matches = regex.match(line)
        if matches:
            return int(matches.groups()[1]) / int(matches.groups()[0])
    return -1


def extract_branch_mpki(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    regex = re.compile(r"BRANCH\_MPKI: (\d*\.?\d+)")
    logs.reverse()
    for line in logs:  # reverse to find last run first
        matches = regex.match(line)
        if matches:
            return matches.groups()[0]


def extract_rob_at_stall(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    regex = re.compile(r"AVG ROB SIZE AT STALL: (\d*\.?\d+)")
    logs.reverse()
    for line in logs:  # reverse to find last run first
        matches = regex.match(line)
        if matches:
            return matches.groups()[0]


def extract_stall_cycles(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
        stallcycles_regex = re.compile(r"CPU 0 FRONTEND STALLED CYCLES:\s+(\d+)")
    regex = re.compile(r"CPU 0 cumulative IPC: (\d*\.?\d+)")
    totalmiss_regex = re.compile(r"cpu0\_L1I TOTAL.*MISS:\s+(\d*)\s+PARTIAL MISS")
    logs.reverse()
    total_misses = -1
    stall_cycles = -1
    for line in logs:
        stallcycles_matches = stallcycles_regex.match(line)
        totalcycles_matches = totalmiss_regex.match(line)
        if stallcycles_matches:
            stall_cycles = int(stallcycles_matches.groups()[0])
        if totalcycles_matches:
            total_misses = int(totalcycles_matches.groups()[0])
    if total_misses < 0 or stall_cycles < 0:
        print("ERROR: DID NOT EXTRACT STALL PERCENTAGE")
        return float("NaN")
    return float(stall_cycles) / float(total_misses)


def extract_l1i_mpki(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    regex = re.compile(r"cpu0\_L1I MPKI: (\d*\.?\d+)")
    logs.reverse()
    for line in logs:  # reverse to find last run first
        matches = regex.match(line)
        if matches:
            return matches.groups()[0]


def extract_frontend_stalls_percentage(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    stallcycles_regex = re.compile(r"CPU 0 FRONTEND STALLED CYCLES:\s+(\d+)")
    regex = re.compile(r"CPU 0 cumulative IPC: (\d*\.?\d+)")
    totalinstructions_regex = re.compile(
        r"CPU 0 cumulative IPC: \d*\.?\d+ instructions: (\d+)"
    )
    logs.reverse()
    total_instructions = -1
    stall_cycles = -1
    for line in logs:
        stallcycles_matches = stallcycles_regex.match(line)
        totalcycles_matches = totalinstructions_regex.match(line)
        if stallcycles_matches:
            stall_cycles = int(stallcycles_matches.groups()[0])
        if totalcycles_matches:
            total_instructions = int(totalcycles_matches.groups()[0])
    if total_instructions < 0 or stall_cycles < 0:
        print("ERROR: DID NOT EXTRACT STALL PERCENTAGE")
        return float("NaN")
    return (float(stall_cycles) / float(total_instructions)) * 1000


def extract_buffer_duration(path):
    assert 0
    return 0


def extract_branch_distance(path):
    logs = []
    with open(path) as f:
        logs = f.readlines()
    candidate_lines = []
    logs = logs[::-1]
    regex = re.compile(r"^\s+(\d+):\s+(\d+)")
    for logline in logs:
        matches = regex.match(logline)
        if matches:
            candidate_lines.append((int(matches.groups()[0]), int(matches.groups()[1])))
        if "BRANCH DISTANCE STATS" in logline:
            break
    buckets = [32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, ">"]
    count_by_bucket = {
        32: 0,
        64: 0,
        128: 0,
        256: 0,
        512: 0,
        1024: 0,
        2048: 0,
        4096: 0,
        8192: 0,
        16384: 0,
        ">": 0,
    }
    for single_entry in candidate_lines:
        old_bucket = 0
        bucket_key = 0
        for bucket in buckets:
            if old_bucket < single_entry[0] and (
                bucket == ">" or single_entry[0] < bucket
            ):
                bucket_key = bucket
                break
        count_by_bucket[bucket_key] += single_entry[1]
    return count_by_bucket


def single_run(path):
    stat_by_workload = {}
    for workload in os.listdir(path):
        if not os.path.isdir(os.path.join(path, workload)):
            continue  # if we do this for a single single run
        if workload in ["graphs", "raw_data"]:
            continue
        for logfile in os.listdir(f"{path}/{workload}"):
            if not ".txt" in logfile or logfile.startswith("."):
                continue
            if type == STATS.MPKI:
                stat_by_workload[workload] = extract_l1i_mpki(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.FETCH_COUNT:
                stat_by_workload[workload] = extract_fetches(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.PARTIAL:
                stat_by_workload[workload] = extract_l1i_partial(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.USELESS:
                stat_by_workload[workload] = extrace_useless_percentage(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.FRONTEND_STALLS:
                stat_by_workload[workload] = extract_frontend_stalls_percentage(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.PARTIAL_MISSES:
                stat_by_workload[workload] = extract_l1i_detail_partial_misses(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.BUFFER_DURATION:
                stat_by_workload[workload] = extract_buffer_duration(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.BRANCH_DISTANCES:
                stat_by_workload[workload] = extract_branch_distance(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.BRANCH_COUNT:
                stat_by_workload[workload] = extract_branch_count(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.BIG_TARGET_COUNT:
                stat_by_workload[workload] = extract_big_offset_counts(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.INSTRUCTION_COUNT:
                stat_by_workload[workload] = extract_instruction_count(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.CONTEXT_SWITCH:
                stat_by_workload[workload] = extract_context_switch_count(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.NUM_BTB_BITS_PER_CL:
                stat_by_workload[workload] = extract_btb_bits_per_cl(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.ALIASING:
                stat_by_workload[workload] = extract_positive_btb_aliasing(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.ABSOLUTE_ALIASING:
                stat_by_workload[workload] = extract_absolute_btb_aliasing(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.BIT_INFORMATION:
                stat_by_workload[workload] = extract_bit_information(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.BTB_TAG_ENTROPY:
                stat_by_workload[workload] = extract_btb_tag_entropy(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.REGION_SPLIT:
                stat_by_workload[workload] = extract_btb_region_split(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.BTB_BIT_ORDERING:
                bit_ordering = extract_btb_bit_ordering(f"{path}/{workload}/{logfile}")
                raw_data_path = f"{path}/{workload}_bit_ordering.txt"
                print(f"BIT ORDERING written to {raw_data_path}")
                with open(raw_data_path, "x") as f:
                    for bit in bit_ordering:
                        f.write(f"{bit}\n")
            elif type == STATS.ROB_AT_MISS:
                stat_by_workload[workload] = extract_rob_at_stall(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.BRANCH_MPKI:
                stat_by_workload[workload] = extract_branch_mpki(
                    f"{path}/{workload}/{logfile}"
                )
            elif type == STATS.STALL_CYCLES:
                stat_by_workload[workload] = extract_stall_cycles(
                    f"{path}/{workload}/{logfile}"
                )
            else:
                stat_by_workload[workload] = extract_ipc(f"{path}/{workload}/{logfile}")
    return stat_by_workload


def mutliple_sizes_run(out_dir=None):
    ipc_by_cachesize_and_workload = {}
    if not out_dir:
        out_dir = sys.argv[1]
    for subdir in os.listdir(out_dir):
        if not os.path.isdir(os.path.join(out_dir, subdir)):
            continue
        lip_matches = re.search(r"(\d*)lip", subdir)
        matches = re.search(r"(\d+)([km])+$", subdir)
        if not matches:
            result = single_run(f"{out_dir}/{subdir}")
            if len(result) == 0:
                continue
            ipc_by_cachesize_and_workload[subdir] = result
            continue
        matches = matches.groups()
        name = ""
        if lip_matches:
            name = f"lip@{lip_matches.groups()[0]} "
        if matches[1] and not "fdip" in subdir:
            factor = 1024 if matches[1] == "k" else 1024 * 1024
            size_bytes = int(matches[0]) * factor
            name += f"{size_bytes}"
        else:
            name = subdir
        result = single_run(f"{out_dir}/{subdir}")
        if len(result) == 0:
            continue
        ipc_by_cachesize_and_workload[name] = result
    return ipc_by_cachesize_and_workload


def write_partial_misses(data, out_path="./"):
    base_filename = "partial_misses_"
    partial_miss_causes = ["UNDERRUNS", "OVERRUNS", "MERGES", "NEW BLOCKS"]
    for csize, values in data.items():
        filename = base_filename + str(csize)
        filename += ".tsv"
        file_path = os.path.join(out_path, filename)
        if not ("ubs" in csize or "vcl" in csize):
            continue
        with open(file_path, "w+") as outfile:
            for workload, _ in data[csize].items():
                outfile.write(f"\t{workload}")
            outfile.write("\n")
            for idx, partial_miss_cause in enumerate(partial_miss_causes):
                outfile.write(f"{partial_miss_cause}")
                for workload, val in data[csize].items():
                    outfile.write(f"\t{val[idx]}")
                outfile.write("\n")
            outfile.flush()
    return


def write_series(data, out_path="./"):
    base_filename = "branch_distances"
    for csize, values in data.items():
        filename = base_filename + str(csize)
        filename += ".tsv"
        file_path = os.path.join(out_path, filename)
        with open(file_path, "w+") as outfile:
            for workload, series in data[csize].items():
                outfile.write(f"{workload}")
                for _, entry in series.items():
                    outfile.write(f"\t{entry}")
                outfile.write("\n")
            outfile.flush()


def write_tsv(data, out_path=None):
    filename = "ipc"
    if type == STATS.MPKI:
        filename = "mpki"
    elif type == STATS.ALIASING:
        filename = "aliasing"
    elif type == STATS.ABSOLUTE_ALIASING:
        filename = "total_aliasing"
    elif type == STATS.FETCH_COUNT:
        filename = "fetch_count"
    elif type == STATS.BRANCH_MPKI:
        filename = "branch_mpki"
    elif type == STATS.PARTIAL:
        filename = "partial"
    elif type == STATS.BUFFER_DURATION:
        filename = "avg_buffer"
    elif type == STATS.USELESS:
        filename = "useless"
    elif type == STATS.FRONTEND_STALLS:
        filename = "frontend_stalls"
    elif type == STATS.PARTIAL_MISSES:
        filename = "partial_misses"
    elif type == STATS.BRANCH_DISTANCES:
        filename = "branch_distances"
    elif type == STATS.BRANCH_COUNT:
        filename = "branch_count"
    elif type == STATS.BIG_TARGET_COUNT:
        filename = "big_target"
    elif type == STATS.INSTRUCTION_COUNT:
        filename = "instruction_count"
    elif type == STATS.CONTEXT_SWITCH:
        filename = "context_switches"
    elif type == STATS.STALL_CYCLES:
        filename = "stall_cycles"
    elif type == STATS.ROB_AT_MISS:
        filename = "rob_at_miss"
    elif type == STATS.NUM_BTB_BITS_PER_CL:
        filename = "num_btb_bits_per_cacheline"
    elif type == STATS.BIT_INFORMATION:
        filename = "address_bit_information"
    elif type == STATS.BTB_TAG_ENTROPY:
        filename = "btb_tag_entropy"
    elif type == STATS.REGION_SPLIT:
        filename = "btb_region_tag_split"
    if buffer:
        filename += "_buffer"
    filename += ".tsv"
    if out_path:
        out_path = os.path.join(out_path, filename)
    else:
        out_path = os.path.join("./", filename)
    with open(out_path, "w+") as outfile:
        max_csize = 0
        abs_max = 0
        for csize, values in data.items():
            if len(values) > abs_max:
                abs_max = len(values)
                max_csize = csize
        headers = []
        count = 0
        for workload, _ in data[max_csize].items():
            outfile.write(f"\t{workload}")
            count += 1
            headers.append(workload)
        outfile.write("\n")
        if type == STATS.PARTIAL_MISSES:
            for i in range(int(count / 4)):
                outfile.write("\tUNDERRUNS\tOVERRUNS\tMERGES\tNEW BLOCKS")
        outfile.write("\n")
        for csize, values in data.items():
            outfile.write(f"{csize}")
            for header in headers:
                val = data[csize].get(header)
                if type == STATS.PARTIAL_MISSES:
                    if val == None:
                        val = [0, 0, 0, 0]
                    for i in range(0, 4):
                        outfile.write(f"\t{val[i]}")
                else:
                    outfile.write(f"\t{val}")
            outfile.write("\n")


def multiple_benchmarks_run():
    curr_dir = sys.argv[1]
    for benchmark in os.listdir(curr_dir):
        benchpath = os.path.join(curr_dir, benchmark)
        if not os.path.isdir(benchpath):
            continue
        data = mutliple_sizes_run(out_dir=benchpath)
        if len(data) == 0:
            continue
        write_tsv(data, out_path=benchpath)


data = {}
if sys.argv[3] == "MPKI":
    type = STATS.MPKI
elif sys.argv[3] == "FETCH_COUNT":
    type = STATS.FETCH_COUNT
elif sys.argv[3] == "BRANCH_MPKI":
    type = STATS.BRANCH_MPKI
elif sys.argv[3] == "STALL_CYCLES":
    type = STATS.STALL_CYCLES
elif sys.argv[3] == "ROB_AT_MISS":
    type = STATS.ROB_AT_MISS
elif sys.argv[3] == "PARTIAL":
    type = STATS.PARTIAL
elif sys.argv[3] == "BUFFER_DURATION":
    type = STATS.BUFFER_DURATION
elif sys.argv[3] == "USELESS_LINES":
    type = STATS.USELESS
elif sys.argv[3] == "FRONTEND_STALLS":
    type = STATS.FRONTEND_STALLS
elif sys.argv[3] == "PARTIAL_MISSES":
    type = STATS.PARTIAL_MISSES
elif sys.argv[3] == "BRANCH_DISTANCES":
    type = STATS.BRANCH_DISTANCES
elif sys.argv[3] == "BRANCH_COUNT":
    type = STATS.BRANCH_COUNT
elif sys.argv[3] == "BIG_TARGET_COUNT":
    type = STATS.BIG_TARGET_COUNT
elif sys.argv[3] == "INSTRUCTION_COUNT":
    type = STATS.INSTRUCTION_COUNT
elif sys.argv[3] == "CONTEXT_SWITCH":
    type = STATS.CONTEXT_SWITCH
elif sys.argv[3] == "BTB_BITS_CL":
    type = STATS.NUM_BTB_BITS_PER_CL
elif sys.argv[3] == "BTB_ALIASING":
    type = STATS.ALIASING
elif sys.argv[3] == "BTB_TOTAL_ALIASING":
    type = STATS.ABSOLUTE_ALIASING
elif sys.argv[3] == "BTB_BIT_INFORMATION":
    type = STATS.BIT_INFORMATION
elif sys.argv[3] == "BTB_TAG_ENTROPY":
    type = STATS.BTB_TAG_ENTROPY
elif sys.argv[3] == "REGION_SPLIT":
    type = STATS.REGION_SPLIT
elif sys.argv[3] == "BTB_BIT_ORDERING":
    type = STATS.BTB_BIT_ORDERING
if len(sys.argv) == 5 and sys.argv[4]:
    buffer = True
if sys.argv[2] == "single":
    data["const"] = single_run(sys.argv[1])
elif sys.argv[2] == "multibench":
    multiple_benchmarks_run()
    exit(0)
else:
    data = mutliple_sizes_run()

if len(data) == 0:
    exit(0)

if type == STATS.PARTIAL_MISSES:
    write_partial_misses(data, sys.argv[1])
elif type == STATS.BRANCH_DISTANCES:
    write_series(data, sys.argv[1])
elif type == STATS.NUM_BTB_BITS_PER_CL:
    file_path = os.path.join(sys.argv[1], "num_btb_bits_per_cl.tsv")
    with open(file_path, "w+") as outfile:
        for workload, values in data["const"].items():
            outfile.write(f"{workload}")
            for entry in values:
                outfile.write(f"\t{entry}")
            outfile.write("\n")
        outfile.flush()
# elif type == STATS.ALIASING:
#     file_path = os.path.join(sys.argv[1], "aliasing.tsv")
#     with open(file_path, "w+") as outfile:
#         outfile.write(
#             "Workload\tTotal Lookups\tAliasing Lookups\tSame Block Aliases\tDifferent Block Aliases\n"
#         )
#         for workload, values in data["const"].items():
#             outfile.write(f"{workload}")
#             for entry in values:
#                 outfile.write(f"\t{entry}")
#             outfile.write("\n")
#         outfile.flush()
elif type == STATS.BIT_INFORMATION:
    file_path = os.path.join(sys.argv[1], "btb_dynamic_bit_information.tsv")
    with open(file_path, "w+") as outfile:
        outfile.write("Workload")
        for i in range(1, 65):
            outfile.write(f"\t{i}")
        outfile.write("\n")
        for workload, values in data["const"].items():
            outfile.write(f"{workload}")
            for entry in values:
                outfile.write(f"\t{entry}")
            outfile.write("\n")
        outfile.flush()
else:
    write_tsv(data, sys.argv[1])
