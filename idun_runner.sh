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
module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0
#binaries=("champsim64k" "champsim32k" "champsim128m" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d_arm" "champsim_vcl_buffer_16")
#binaries=("champsim64k" "champsim32k" "champsim128m" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d_arm" "champsim_vcl_buffer_16")
#binaries=("champsim32k" "ubs_10_small_ways_not_extending" "ubs_10_small_ways" "ubs_10_ways_not_extending" "ubs_10_ways" "ubs_12_small_ways_not_extending" "ubs_12_small_ways" "ubs_14_small_ways_not_extending" "ubs_14_small_ways" "ubs_16_ways" "ubs_16_ways_not_extending" "ubs_17_ways" "ubs_17_ways_not_extending" "ubs_8_ways" "ubs_8_ways_not_extending")
#binaries=("champsim32k" "ubs_12_ways_not_extending" "ubs_12_ways" "ubs_14_ways" "ubs_14_small_ways" "ubs_12_small_ways_not_extending" "ubs_12_small_ways" "ubs_14_small_ways_not_extending" "ubs_14_small_ways" "ubs_16_ways_precise" "ubs_16_ways_precise_not_extending")
#binaries=("champsim32k_base_btb" "champsim32k_base_btbx" "champsim32k_hash_btbx" "champsim32k_perfect_l1i" "champsim32k_perfect_btb")
binary_dir=("offset_saturation")
#binary_dir=("size_sensitivity")
#binaries=("ubs" "ubs_unaligned" "ubs_extended" "ubs_unaligned_extended")
#binaries=("ubs_overhead_isca_extend_lru" "ubs_overhead_isca_lru")

for dir in ${binary_dir[@]}
do
    for bin in ~/ChampSim/bin/$dir/*
    do
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t01:00:00 -o /cluster/work/romankb/latency-spec-$(basename ${bin})-%j.out    -e /cluster/work/romankb/latency-spec-$(basename ${bin})-%j.err    python ~/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable ${bin}         --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/spec   --nosub --output_dir /cluster/work/romankb/results/${dir}_clip_and_fold/ipc1_spec/sizes_$(basename ${bin})/      >> /cluster/work/romankb/pyrunner_latency_fixed_ipc1_spec_$(basename ${bin}).log &
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t01:00:00 -o /cluster/work/romankb/latency-client-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-client-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable ${bin}         --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/client --nosub --output_dir /cluster/work/romankb/results/${dir}_clip_and_fold/ipc1_client/sizes_$(basename ${bin})/    >> /cluster/work/romankb/pyrunner_latency_fixed_ipc1_client_$(basename ${bin}).log &
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t01:00:00 -o /cluster/work/romankb/latency-server-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-server-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable ${bin}         --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/server --nosub --output_dir /cluster/work/romankb/results/${dir}_clip_and_fold/ipc1_server/sizes_$(basename ${bin})/    >> /cluster/work/romankb/pyrunner_latency_fixed_ipc1_server_$(basename ${bin}).log &
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t01:00:00 -o /cluster/work/romankb/latency-whiskey-$(basename ${bin})-%j.out -e /cluster/work/romankb/latency-whiskey-$(basename ${bin})-%j.err python ~/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/google/whiskey             --nosub --output_dir /cluster/work/romankb/results/${dir}_clip_and_fold/google_whiskey/sizes_$(basename ${bin})/ >> /cluster/work/romankb/pyrunner_latency_fixed_google_whiskey_$(basename ${bin}).log &
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t01:00:00 -o /cluster/work/romankb/latency-delta-$(basename ${bin})-%j.out   -e /cluster/work/romankb/latency-delta-$(basename ${bin})-%j.err   python ~/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/google/delta               --nosub --output_dir /cluster/work/romankb/results/${dir}_clip_and_fold/google_delta/sizes_$(basename ${bin})/   >> /cluster/work/romankb/pyrunner_latency_fixed_google_delta_$(basename ${bin}).log &
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t01:00:00 -o /cluster/work/romankb/latency-charlie-$(basename ${bin})-%j.out -e /cluster/work/romankb/latency-charlie-$(basename ${bin})-%j.err python ~/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/google/charlie             --nosub --output_dir /cluster/work/romankb/results/${dir}_clip_and_fold/google_charlie/sizes_$(basename ${bin})/ >> /cluster/work/romankb/pyrunner_latency_fixed_google_charlie_$(basename ${bin}).log &
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t01:00:00 -o /cluster/work/romankb/latency-merced-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-merced-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/google/merced              --nosub --output_dir /cluster/work/romankb/results/${dir}_clip_and_fold/google_merced/sizes_$(basename ${bin})/  >> /cluster/work/romankb/pyrunner_latency_fixed_google_merced_$(basename ${bin}).log &
    done
done

for job in `jobs -p`
do
    echo $job
    wait $job
done
