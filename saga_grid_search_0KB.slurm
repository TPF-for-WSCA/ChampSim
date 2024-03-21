#!/bin/bash
#SBATCH --job-name="num-collection-champsim-grid"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c8
#SBATCH --mem-per-cpu=20G
#SBATCH --time=01-00:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/users/romankb/0KB_grid-%j.err
#SBATCH -o /cluster/work/users/romankb/0KB_grid-%j.out

module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0

binaries0KB=("champsim32k" "champsim64k" "saga_vcl_0KB_b_bwbp128" "saga_vcl_0KB_b_bwbp16" "saga_vcl_0KB_b_bwbp256" "saga_vcl_0KB_b_bwbp32" "saga_vcl_0KB_b_bwbp512" "saga_vcl_0KB_b_bwbp64" "saga_vcl_0KB_b_NE" "saga_vcl_0KB_p_bwbp128" "saga_vcl_0KB_p_bwbp16" "saga_vcl_0KB_p_bwbp256" "saga_vcl_0KB_p_bwbp32" "saga_vcl_0KB_p_bwbp512" "saga_vcl_0KB_p_bwbp64" "saga_vcl_0KB_p_NE" "saga_vcl_0KB_s_bwbp128" "saga_vcl_0KB_s_bwbp16" "saga_vcl_0KB_s_bwbp256" "saga_vcl_0KB_s_bwbp32" "saga_vcl_0KB_s_bwbp512" "saga_vcl_0KB_s_bwbp64" "saga_vcl_0KB_s_NE")

for binary in ${binaries0KB[@]}
do
    python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 5000000000 --experiment_executable /cluster/projects/nn4650k/workspace/ChampSim/bin/0KB/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/spec --nosub --output_dir /cluster/work/users/romankb/results/grid_new_v/0KB/ipc1_spec/sizes_${binary}/ >> ~/pyrunner_ipc1_spec_${binary}.log & # We got spec in dpc3
    python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 5000000000 --experiment_executable /cluster/projects/nn4650k/workspace/ChampSim/bin/0KB/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/server --nosub --output_dir /cluster/work/users/romankb/results/grid_new_v/0KB/ipc1_server/sizes_${binary}/ >> ~/pyrunner_ipc1_server_${binary}.log & # We got spec in dpc3
    python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 5000000000 --experiment_executable /cluster/projects/nn4650k/workspace/ChampSim/bin/0KB/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/client --nosub --output_dir /cluster/work/users/romankb/results/grid_new_v/0KB/ipc1_client/sizes_${binary}/ >> ~/pyrunner_ipc1_client_${binary}.log & # We got spec in dpc3
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

