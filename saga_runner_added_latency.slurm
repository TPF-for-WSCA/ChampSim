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
binaries=("champsim32k_8B" "champsim32k_unaligned" "champsim32k_32B" "champsim32k" "champsim64k" "ubs_16B" "ubs_32B" "ubs" "ubs_unaligned" "ubs_16B_extended" "ubs_32B_extended" "ubs_extended" "ubs_unaligned_extended" "ubs_unaligned_overhead" "ubs_overhead")

for binary in ${binaries[@]}
do
    python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 500000000 --experiment_executable /cluster/projects/nn4650k/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/spec --nosub --output_dir   /cluster/work/users/romankb/results/latency_grid_5/ipc1_spec/sizes_${binary}/   >> /cluster/work/users/romankb/pyrunner_latency_fixed_ipc1_spec_${binary}.log &
    python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 500000000 --experiment_executable /cluster/projects/nn4650k/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/server --nosub --output_dir /cluster/work/users/romankb/results/latency_grid_5/ipc1_server/sizes_${binary}/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_ipc1_server_${binary}.log &
    python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 500000000 --experiment_executable /cluster/projects/nn4650k/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/client --nosub --output_dir /cluster/work/users/romankb/results/latency_grid_5/ipc1_client/sizes_${binary}/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_ipc1_client_${binary}.log &
done

for job in `jobs -p`
do
    echo $job
    wait $job
done
