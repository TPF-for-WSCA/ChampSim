#!/bin/bash
#SBATCH --job-name="num-collection-base-btb"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c20
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00-24:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=NONE
#SBATCH -e /cluster/work/users/romankb/latency-%j.err
#SBATCH -o /cluster/work/users/romankb/latency-%j.out

module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0
#binaries=("champsim64k" "champsim32k" "champsim128m" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d_arm" "champsim_vcl_buffer_16")
#binaries=("champsim64k" "champsim32k" "champsim128m" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d_arm" "champsim_vcl_buffer_16")
#binaries=("champsim32k" "ubs_10_small_ways_not_extending" "ubs_10_small_ways" "ubs_10_ways_not_extending" "ubs_10_ways" "ubs_12_small_ways_not_extending" "ubs_12_small_ways" "ubs_14_small_ways_not_extending" "ubs_14_small_ways" "ubs_16_ways" "ubs_16_ways_not_extending" "ubs_17_ways" "ubs_17_ways_not_extending" "ubs_8_ways" "ubs_8_ways_not_extending")
#binaries=("champsim32k" "ubs_12_ways_not_extending" "ubs_12_ways" "ubs_14_ways" "ubs_14_small_ways" "ubs_12_small_ways_not_extending" "ubs_12_small_ways" "ubs_14_small_ways_not_extending" "ubs_14_small_ways" "ubs_16_ways_precise" "ubs_16_ways_precise_not_extending")
#binaries=("champsim32k_base_btb" "champsim32k_base_btbx" "champsim32k_hash_btbx" "champsim32k_perfect_l1i" "champsim32k_perfect_btb")
binary_dir=("wrong_path")
#binary_dir=("size_sensitivity")
#binaries=("ubs" "ubs_unaligned" "ubs_extended" "ubs_unaligned_extended")
#binaries=("ubs_overhead_isca_extend_lru" "ubs_overhead_isca_lru")

for dir in ${binary_dir[@]}
do
    for bin in /cluster/projects/nn4650k/workspace/ChampSim/bin/$dir/*
    do
        srun --account=nn4650k -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c10 -t02:00:00 -o /cluster/work/users/romankb/latency-%j.out -e /cluster/work/users/romankb/latency-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable ${bin} --traces_directory /cluster/home/romankb/dataset/ipc1_new/spec --nosub --output_dir   /cluster/work/users/romankb/results/micro_rebuttal_predictor_sensitivity/ipc1_spec/sizes_$(basename ${bin})/   >> /cluster/work/users/romankb/pyrunner_latency_fixed_ipc1_spec_$(basename ${bin}).log &
        srun --account=nn4650k -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c10 -t02:00:00 -o /cluster/work/users/romankb/latency-%j.out -e /cluster/work/users/romankb/latency-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable ${bin} --traces_directory /cluster/home/romankb/dataset/ipc1_new/client --nosub --output_dir /cluster/work/users/romankb/results/micro_rebuttal_predictor_sensitivity/ipc1_client/sizes_$(basename ${bin})/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_ipc1_client_$(basename ${bin}).log &
        srun --account=nn4650k -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c10 -t02:00:00 -o /cluster/work/users/romankb/latency-%j.out -e /cluster/work/users/romankb/latency-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable ${bin} --traces_directory /cluster/home/romankb/dataset/ipc1_new/server --nosub --output_dir /cluster/work/users/romankb/results/micro_rebuttal_predictor_sensitivity/ipc1_server/sizes_$(basename ${bin})/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_ipc1_server_$(basename ${bin}).log &
        # srun --account=nn4650k -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c10 -t01:30:00 -o /cluster/work/users/romankb/latency-%j.out -e /cluster/work/users/romankb/latency-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 5000000 --evaluation 50000000 --experiment_executable ${bin} --traces_directory /cluster/home/romankb/dataset/cvp1_new/server --nosub --output_dir  /cluster/work/users/romankb/results/micro_new_1/cvp1_new_server/sizes_$(basename ${bin})/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_cvp1_server_$(basename ${bin}).log &
        # srun --account=nn4650k -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c10 -t01:30:00 -o /cluster/work/users/romankb/latency-%j.out -e /cluster/work/users/romankb/latency-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 5000000 --evaluation 50000000 --experiment_executable ${bin} --traces_directory /cluster/home/romankb/dataset/cvp1_new/compute --nosub --output_dir /cluster/work/users/romankb/results/micro_new_1/cvp1_new_compute/sizes_$(basename ${bin})/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_cvp1_compute_$(basename ${bin}).log &
    done
done

for job in `jobs -p`
do
    echo $job
    wait $job
done
 
