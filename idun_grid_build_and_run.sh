#!/bin/bash

# For this script to work, the config and the binary have to be named identically

# Build configs
build_dir=("equiperformance_configs") # "4k_12b_region_tag_sensitivity" "4k_10b_region_tag_sensitivity") #  "btb_512_region_tag_exp" "btb_1k_region_tag_exp" "btb_2k_region_tag_exp" "btb_4k_region_tag_exp" "btb_8k_region_tag_exp" "btb_256_region_tag_exp")
# build_dir=("btb_512_region_tag_exp" "btb_1k_region_tag_exp" "btb_2k_region_tag_exp" "btb_4k_region_tag_exp" "btb_8k_region_tag_exp")

# Run configs
binary_dir=("equiperformance")
suffix="with_data_prefetcher"
count=2
timelimit="4:00:00"
warmup=50000000
simulation=50000000

old_dir=$(pwd)
cd ~/ChampSim/
for spec_dir in ${build_dir[@]}
do
    echo "Building experiment $spec_dir"
    for build_script in ./IDUN_CONFIGS/$spec_dir/*;
    do
        if ! [[ -f "$build_script" ]]; then
            continue
        fi
        echo -e "\tConfiguring ${build_script}"
        ./config.sh $build_script
        echo -e "\tBuilding ${build_script}"
        make -j &>> /cluster/work/romankb/build_$(basename ${build_script%.json}).log
        bin="~/ChampSim/bin/$spec_dir/$(basename $build_script)"
        echo "Run $bin"
 # IPC-1 Benchmarks
        # srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-spec-$(basename ${bin})-%j.out    -e /cluster/work/romankb/latency-spec-$(basename ${bin})-%j.err    python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/spec   --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/ipc1_spec/sizes_$(basename ${bin})/              &>> /cluster/work/romankb/pyrunner_latency_fixed_ipc1_spec_$(basename ${bin}).log &
        # srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-client-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-client-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/client --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/ipc1_client/sizes_$(basename ${bin})/            &>> /cluster/work/romankb/pyrunner_latency_fixed_ipc1_client_$(basename ${bin}).log &
        # srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-server-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-server-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/server --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/ipc1_server/sizes_$(basename ${bin})/            &>> /cluster/work/romankb/pyrunner_latency_fixed_ipc1_server_$(basename ${bin}).log &
        # SPECCPU / DPC-3 Benchmarks # Deactivated for now - most have little impact, biggest impact however also here
        # srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-server-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-server-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/dpc3                       --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/dpc3/sizes_$(basename ${bin})/           &>> /cluster/work/romankb/pyrunner_latency_fixed_dpc3_$(basename ${bin}).log &
        # LLBP Benchmarks --intel
        # srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-server-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-server-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/LLBP                       --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/LLBP/sizes_$(basename ${bin})/           &>> /cluster/work/romankb/pyrunner_latency_fixed_llbp_$(basename ${bin}).log &
        # Google Traces / No performance benchmark as no dependency information --intel
        # srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-whiskey-$(basename ${bin})-%j.out -e /cluster/work/romankb/latency-whiskey-$(basename ${bin})-%j.err python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/google/whiskey             --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/google_whiskey/sizes_$(basename ${bin})/ &>> /cluster/work/romankb/pyrunner_latency_fixed_google_whiskey_$(basename ${bin}).log &
        # srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-delta-$(basename ${bin})-%j.out   -e /cluster/work/romankb/latency-delta-$(basename ${bin})-%j.err   python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/google/delta               --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/google_delta/sizes_$(basename ${bin})/   &>> /cluster/work/romankb/pyrunner_latency_fixed_google_delta_$(basename ${bin}).log &
        # srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-charlie-$(basename ${bin})-%j.out -e /cluster/work/romankb/latency-charlie-$(basename ${bin})-%j.err python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/google/charlie             --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/google_charlie/sizes_$(basename ${bin})/ &>> /cluster/work/romankb/pyrunner_latency_fixed_google_charlie_$(basename ${bin}).log &
        # srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-merced-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-merced-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/google/merced              --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/google_merced/sizes_$(basename ${bin})/  &>> /cluster/work/romankb/pyrunner_latency_fixed_google_merced_$(basename ${bin}).log &
    done
done
cd $old_dir
