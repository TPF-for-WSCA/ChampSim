#!/bin/bash
#SBATCH --job-name="num-collection-base-btb"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c20
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00-24:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/users/romankb/latency-%j.err
#SBATCH -o /cluster/work/users/romankb/latency-%j.out

module load Python/3.12.3-GCCcore-13.3.0
module load GCC/13.3.0
module load GCCcore/13.3.0
#binaries=("champsim64k" "champsim32k" "champsim128m" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d_arm" "champsim_vcl_buffer_16")
#binaries=("champsim64k" "champsim32k" "champsim128m" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d_arm" "champsim_vcl_buffer_16")
#binaries=("champsim32k" "ubs_10_small_ways_not_extending" "ubs_10_small_ways" "ubs_10_ways_not_extending" "ubs_10_ways" "ubs_12_small_ways_not_extending" "ubs_12_small_ways" "ubs_14_small_ways_not_extending" "ubs_14_small_ways" "ubs_16_ways" "ubs_16_ways_not_extending" "ubs_17_ways" "ubs_17_ways_not_extending" "ubs_8_ways" "ubs_8_ways_not_extending")
#binaries=("champsim32k" "ubs_12_ways_not_extending" "ubs_12_ways" "ubs_14_ways" "ubs_14_small_ways" "ubs_12_small_ways_not_extending" "ubs_12_small_ways" "ubs_14_small_ways_not_extending" "ubs_14_small_ways" "ubs_16_ways_precise" "ubs_16_ways_precise_not_extending")
#binaries=("champsim32k_base_btb" "champsim32k_base_btbx" "champsim32k_hash_btbx" "champsim32k_perfect_l1i" "champsim32k_perfect_btb")
binary_dir=("btb_region_tag")
count=5
#binary_dir=("size_sensitivity")
#binaries=("ubs" "ubs_unaligned" "ubs_extended" "ubs_unaligned_extended")
#binaries=("ubs_overhead_isca_extend_lru" "ubs_overhead_isca_lru")

for dir in ${binary_dir[@]}
do
    for bin in /cluster/projects/nn4650k/workspace/ChampSim/bin/$dir/*
    do
        # IPC-1 Benchmarks
        srun --account=nn4650k -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t05:00:00 -o /cluster/work/users/romankb/latency-spec-$(basename ${bin})-%j.out   -e /cluster/work/users/romankb/latency-spec-$(basename ${bin})-%j.err     python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 15000000 --evaluation 15000000 --experiment_executable ${bin} --traces_directory /cluster/projects/nn4650k/dataset/ipc1_new/spec   --nosub --output_dir /cluster/work/users/romankb/results/${dir}_${count}/ipc1_spec/sizes_$(basename ${bin})/   >> /cluster/work/users/romankb/pyrunner_latency_fixed_ipc1_spec_$(basename ${bin}).log &
        srun --account=nn4650k -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t05:00:00 -o /cluster/work/users/romankb/latency-client-$(basename ${bin})-%j.out -e /cluster/work/users/romankb/latency-client-$(basename ${bin})-%j.err   python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 15000000 --evaluation 15000000 --experiment_executable ${bin} --traces_directory /cluster/projects/nn4650k/dataset/ipc1_new/client --nosub --output_dir /cluster/work/users/romankb/results/${dir}_${count}/ipc1_client/sizes_$(basename ${bin})/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_ipc1_client_$(basename ${bin}).log &
        srun --account=nn4650k -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t05:00:00 -o /cluster/work/users/romankb/latency-server-$(basename ${bin})-%j.out -e /cluster/work/users/romankb/latency-server-$(basename ${bin})-%j.err   python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 15000000 --evaluation 15000000 --experiment_executable ${bin} --traces_directory /cluster/projects/nn4650k/dataset/ipc1_new/server --nosub --output_dir /cluster/work/users/romankb/results/${dir}_${count}/ipc1_server/sizes_$(basename ${bin})/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_ipc1_server_$(basename ${bin}).log &
        # SPECCPU / DPC-3 Benchmarks
        srun --account=nn4650k -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t05:00:00 -o /cluster/work/users/romankb/latency-speccpu-$(basename ${bin})-%j.out -e /cluster/work/users/romankb/latency-speccpu-$(basename ${bin})-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 15000000 --evaluation 15000000 --experiment_executable ${bin} --traces_directory /cluster/projects/nn4650k/dataset/speccpu --nosub --output_dir /cluster/work/users/romankb/results/${dir}_${count}/speccpu/sizes_$(basename ${bin})/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_speccpu_$(basename ${bin}).log &
        # LLBP Benchmarks
        srun --account=nn4650k -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=20G -n1 -c8 -t05:00:00 -o /cluster/work/users/romankb/latency-llbp-$(basename ${bin})-%j.out -e /cluster/work/users/romankb/latency-llbp-$(basename ${bin})-%j.err       python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 15000000 --evaluation 15000000 --experiment_executable ${bin} --traces_directory /cluster/projects/nn4650k/dataset/LLBP    --nosub --output_dir /cluster/work/users/romankb/results/${dir}_${count}/llbp/sizes_$(basename ${bin})/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_llbp_$(basename ${bin}).log &
        # Google Traces / No performance benchmark as no dependency information
        # srun --account=nn4650k -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t03:00:00 -o /cluster/work/users/romankb/latency-server-$(basename ${bin})-%j.out -e /cluster/work/users/romankb/latency-server-$(basename ${bin})-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable ${bin} --traces_directory /cluster/projects/nn4650k/dataset/google/delta    --nosub --output_dir /cluster/work/users/romankb/results/${dir}_${count}/google_delta/sizes_$(basename ${bin})/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_google_delta_$(basename ${bin}).log &
        # srun --account=nn4650k -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t03:00:00 -o /cluster/work/users/romankb/latency-server-$(basename ${bin})-%j.out -e /cluster/work/users/romankb/latency-server-$(basename ${bin})-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable ${bin} --traces_directory /cluster/projects/nn4650k/dataset/google/whiskey  --nosub --output_dir /cluster/work/users/romankb/results/${dir}_${count}/google_whiskey/sizes_$(basename ${bin})/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_google_whiskey_$(basename ${bin}).log &
        # srun --account=nn4650k -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t03:00:00 -o /cluster/work/users/romankb/latency-server-$(basename ${bin})-%j.out -e /cluster/work/users/romankb/latency-server-$(basename ${bin})-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable ${bin} --traces_directory /cluster/projects/nn4650k/dataset/google/charlie  --nosub --output_dir /cluster/work/users/romankb/results/${dir}_${count}/google_charlie/sizes_$(basename ${bin})/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_google_charlie_$(basename ${bin}).log &
        # srun --account=nn4650k -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t03:00:00 -o /cluster/work/users/romankb/latency-server-$(basename ${bin})-%j.out -e /cluster/work/users/romankb/latency-server-$(basename ${bin})-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable ${bin} --traces_directory /cluster/projects/nn4650k/dataset/google/merced   --nosub --output_dir /cluster/work/users/romankb/results/${dir}_${count}/google_merced/sizes_$(basename ${bin})/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_google_merced_$(basename ${bin}).log &
    done
done

for job in `jobs -p`
do
    echo $job
    wait $job
done
