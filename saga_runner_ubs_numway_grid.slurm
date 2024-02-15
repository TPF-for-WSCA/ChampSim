#!/bin/bash
#SBATCH --job-name="num-collection-champsim-quick"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c16
#SBATCH --mem-per-cpu=10G
#SBATCH --time=00-24:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/users/romankb/small-grid-%j.err
#SBATCH -o /cluster/work/users/romankb/small-grid-%j.out

module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0
#binaries=("champsim64k" "champsim32k" "champsim128m" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d_arm" "champsim_vcl_buffer_16")
#binaries=("champsim64k" "champsim32k" "champsim128m" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d_arm" "champsim_vcl_buffer_16")
#binaries=("champsim32k" "ubs_10_small_ways_not_extending" "ubs_10_small_ways" "ubs_10_ways_not_extending" "ubs_10_ways" "ubs_12_small_ways_not_extending" "ubs_12_small_ways" "ubs_14_small_ways_not_extending" "ubs_14_small_ways")
binaries=("ubs_16_ways" "ubs_16_ways_not_extending")
for binary in ${binaries[@]}
do
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/spec --nosub --output_dir /cluster/work/users/romankb/results/isca_rebuttal_way_ubs_grid/ipc1_spec/sizes_${binary}/ >> ~/pyrunner_small_ubs_ipc1_spec${binary}.log &
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/server --nosub --output_dir /cluster/work/users/romankb/results/isca_rebuttal_way_ubs_grid/ipc1_server/sizes_${binary}/ >> ~/pyrunner_small_ubs_ipc1_server${binary}.log &
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/client --nosub --output_dir /cluster/work/users/romankb/results/isca_rebuttal_way_ubs_grid/ipc1_client/sizes_${binary}/ >> ~/pyrunner_small_ubs_ipc1_client${binary}.log &
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

