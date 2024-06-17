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

module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0
#binaries=("champsim64k" "champsim32k" "champsim128m" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d_arm" "champsim_vcl_buffer_16")
#binaries=("champsim64k" "champsim32k" "champsim128m" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d_arm" "champsim_vcl_buffer_16")
#binaries=("champsim32k" "ubs_10_small_ways_not_extending" "ubs_10_small_ways" "ubs_10_ways_not_extending" "ubs_10_ways" "ubs_12_small_ways_not_extending" "ubs_12_small_ways" "ubs_14_small_ways_not_extending" "ubs_14_small_ways" "ubs_16_ways" "ubs_16_ways_not_extending" "ubs_17_ways" "ubs_17_ways_not_extending" "ubs_8_ways" "ubs_8_ways_not_extending")
#binaries=("champsim32k" "ubs_12_ways_not_extending" "ubs_12_ways" "ubs_14_ways" "ubs_14_small_ways" "ubs_12_small_ways_not_extending" "ubs_12_small_ways" "ubs_14_small_ways_not_extending" "ubs_14_small_ways" "ubs_16_ways_precise" "ubs_16_ways_precise_not_extending")
#binaries=("champsim32k_base_btb" "champsim32k_base_btbx" "champsim32k_hash_btbx" "champsim32k_perfect_l1i" "champsim32k_perfect_btb")
#binaries=("ubs_unaligned_overhead_isca" "ubs_overhead_isca" "ubs" "champsim64k" "champsim32k_unaligned" "champsim32k" "champsim32k_4c_unaligned")
binaries=("champsim_line_distillation")
#binaries=("ubs_overhead_isca_extend_lru" "ubs_overhead_isca_lru")

for binary in /cluster/projects/nn4650k/workspace/ChampSim/bin/micro-rebuttal/${binaries[@]}
do
    srun --account=nn4650k -J num-collection-${binary} --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t01:00:00 -o /cluster/work/users/romankb/latency-%j.out -e /cluster/work/users/romankb/latency-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable ${binary} --traces_directory /cluster/home/romankb/dataset/ipc1/spec --nosub --output_dir   /cluster/work/users/romankb/results/micro_rebuttal_1/ipc1_spec/sizes_$(basename ${binary})/   >> /cluster/work/users/romankb/pyrunner_latency_fixed_ipc1_spec_$(basename ${binary}).log &
    srun --account=nn4650k -J num-collection-${binary} --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t01:00:00 -o /cluster/work/users/romankb/latency-%j.out -e /cluster/work/users/romankb/latency-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable ${binary} --traces_directory /cluster/home/romankb/dataset/ipc1/server --nosub --output_dir /cluster/work/users/romankb/results/micro_rebuttal_1/ipc1_server/sizes_$(basename ${binary})/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_ipc1_server_$(basename ${binary}).log &
    srun --account=nn4650k -J num-collection-${binary} --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t01:00:00 -o /cluster/work/users/romankb/latency-%j.out -e /cluster/work/users/romankb/latency-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable ${binary} --traces_directory /cluster/home/romankb/dataset/ipc1/client --nosub --output_dir /cluster/work/users/romankb/results/micro_rebuttal_1/ipc1_client/sizes_$(basename ${binary})/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_ipc1_client_$(basename ${binary}).log &
done

for job in `jobs -p`
do
    echo $job
    wait $job
done
 
