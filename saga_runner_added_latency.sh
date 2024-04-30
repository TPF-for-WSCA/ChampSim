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
binaries=("champsim32k")
#binaries=("ubs_overhead_isca_extend_lru" "ubs_overhead_isca_lru")

for binary in ${binaries[@]}
do
    srun --account=nn4650k -J num-collection-${binary} --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c7 -t02:00:00 -o /cluster/work/users/romankb/latency-%j.out -e /cluster/work/users/romankb/latency-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/projects/nn4650k/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1/spec --nosub --output_dir   /cluster/work/users/romankb/results/btb_base_data_2/ipc1_spec/sizes_${binary}/   >> /cluster/work/users/romankb/pyrunner_latency_fixed_ipc1_spec_${binary}.log &
    srun --account=nn4650k -J num-collection-${binary} --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c18 -t02:00:00 -o /cluster/work/users/romankb/latency-%j.out -e /cluster/work/users/romankb/latency-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/projects/nn4650k/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1/server --nosub --output_dir /cluster/work/users/romankb/results/btb_base_data_2/ipc1_server/sizes_${binary}/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_ipc1_server_${binary}.log &
    srun --account=nn4650k -J num-collection-${binary} --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c8 -t02:00:00 -o /cluster/work/users/romankb/latency-%j.out -e /cluster/work/users/romankb/latency-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/projects/nn4650k/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1/client --nosub --output_dir /cluster/work/users/romankb/results/btb_base_data_2/ipc1_client/sizes_${binary}/ >> /cluster/work/users/romankb/pyrunner_latency_fixed_ipc1_client_${binary}.log &
done

for job in `jobs -p`
do
    echo $job
    wait $job
done
 
