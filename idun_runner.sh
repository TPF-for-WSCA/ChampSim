#!/bin/bash
#SBATCH --job-name="num-collection-champsim-sizes"
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH -c16
#SBATCH --mem-per-cpu=8G
#SBATCH --time=01-12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/romankb/slurm-default-sizes-%j.err
#SBATCH -o /cluster/work/romankb/slurm-default-sizes-%j.out

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"/cluster/home/romankb/xed/kits/xed-install-base-2023-04-21-lin-x86-64/lib"

module load Python/3.10.8-GCCcore-12.2.0
module load GCC/12.2.0
module load GCCcore/12.2.0
#binaries=("champsim64k" "champsim32k" "champsim128m" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d_arm" "champsim_vcl_buffer_16")
#binaries=("champsim64k" "champsim32k" "champsim128m" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d_arm" "champsim_vcl_buffer_16")
#binaries=("champsim32k" "ubs_10_small_ways_not_extending" "ubs_10_small_ways" "ubs_10_ways_not_extending" "ubs_10_ways" "ubs_12_small_ways_not_extending" "ubs_12_small_ways" "ubs_14_small_ways_not_extending" "ubs_14_small_ways" "ubs_16_ways" "ubs_16_ways_not_extending" "ubs_17_ways" "ubs_17_ways_not_extending" "ubs_8_ways" "ubs_8_ways_not_extending")
#binaries=("champsim32k" "ubs_12_ways_not_extending" "ubs_12_ways" "ubs_14_ways" "ubs_14_small_ways" "ubs_12_small_ways_not_extending" "ubs_12_small_ways" "ubs_14_small_ways_not_extending" "ubs_14_small_ways" "ubs_16_ways_precise" "ubs_16_ways_precise_not_extending")
#binaries=("champsim32k_base_btb" "champsim32k_base_btbx" "champsim32k_hash_btbx" "champsim32k_perfect_l1i" "champsim32k_perfect_btb")
# binary_dir=("btb_region_tag" "btb_4k_region_tag_split_exp" "btb_512_region_tag_exp" "btb_256_region_tag_exp" "btb_1k_region_tag_exp" "btb_2k_region_tag_exp" "btb_4k_region_tag_exp" "btb_8k_region_tag_exp")
binary_dir=("btb_4k_region_tag_split_exp")
suffix="fa_np"
count=0
timelimit="2:30:00"
warmup=30000000
simulation=30000000
#binary_dir=("size_sensitivity")
#binaries=("ubs" "ubs_unaligned" "ubs_extended" "ubs_unaligned_extended")
#binaries=("ubs_overhead_isca_extend_lru" "ubs_overhead_isca_lru")

for dir in ${binary_dir[@]}
do
    echo $dir
    for bin in ~/ChampSim/bin/$dir/*
    do
        echo $bin
        # IPC-1 Benchmarks
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-spec-$(basename ${bin})-%j.out    -e /cluster/work/romankb/latency-spec-$(basename ${bin})-%j.err    python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/spec   --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/ipc1_spec/sizes_$(basename ${bin})/              &>> /cluster/work/romankb/pyrunner_latency_fixed_ipc1_spec_$(basename ${bin}).log &
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-client-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-client-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/client --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/ipc1_client/sizes_$(basename ${bin})/            &>> /cluster/work/romankb/pyrunner_latency_fixed_ipc1_client_$(basename ${bin}).log &
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-server-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-server-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/server --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/ipc1_server/sizes_$(basename ${bin})/            &>> /cluster/work/romankb/pyrunner_latency_fixed_ipc1_server_$(basename ${bin}).log &
        # SPECCPU / DPC-3 Benchmarks # Deactivated for now - most have little impact, biggest impact however also here
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-server-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-server-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/dpc3                       --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/dpc3/sizes_$(basename ${bin})/           &>> /cluster/work/romankb/pyrunner_latency_fixed_dpc3_$(basename ${bin}).log &
        # LLBP Benchmarks --intel
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-server-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-server-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/LLBP                       --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/LLBP/sizes_$(basename ${bin})/           &>> /cluster/work/romankb/pyrunner_latency_fixed_llbp_$(basename ${bin}).log &
        # Google Traces / No performance benchmark as no dependency information --intel
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-whiskey-$(basename ${bin})-%j.out -e /cluster/work/romankb/latency-whiskey-$(basename ${bin})-%j.err python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/google/whiskey             --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/google_whiskey/sizes_$(basename ${bin})/ &>> /cluster/work/romankb/pyrunner_latency_fixed_google_whiskey_$(basename ${bin}).log &
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-delta-$(basename ${bin})-%j.out   -e /cluster/work/romankb/latency-delta-$(basename ${bin})-%j.err   python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/google/delta               --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/google_delta/sizes_$(basename ${bin})/   &>> /cluster/work/romankb/pyrunner_latency_fixed_google_delta_$(basename ${bin}).log &
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-charlie-$(basename ${bin})-%j.out -e /cluster/work/romankb/latency-charlie-$(basename ${bin})-%j.err python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/google/charlie             --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/google_charlie/sizes_$(basename ${bin})/ &>> /cluster/work/romankb/pyrunner_latency_fixed_google_charlie_$(basename ${bin}).log &
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t$timelimit -o /cluster/work/romankb/latency-merced-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-merced-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/google/merced              --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}_${suffix}/google_merced/sizes_$(basename ${bin})/  &>> /cluster/work/romankb/pyrunner_latency_fixed_google_merced_$(basename ${bin}).log &
    done
done

for job in `jobs -p`
do
    echo $job
    wait $job
done
