#!/bin/bash
#SBATCH --job-name="num-collection-champsim-grid"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c16
#SBATCH --mem-per-cpu=4G
#SBATCH --time=00-08:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/users/romankb/slurm-%j.err
#SBATCH -o /cluster/work/users/romankb/slurm-%j.out

module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0

binaries0KB=("champsim32k" "champsim64k" "saga_vcl_0KB_b_bwbp128" "saga_vcl_0KB_b_bwbp16" "saga_vcl_0KB_b_bwbp256" "saga_vcl_0KB_b_bwbp32" "saga_vcl_0KB_b_bwbp512" "saga_vcl_0KB_b_bwbp64" "saga_vcl_0KB_b_NE" "saga_vcl_0KB_p_bwbp128" "saga_vcl_0KB_p_bwbp16" "saga_vcl_0KB_p_bwbp256" "saga_vcl_0KB_p_bwbp32" "saga_vcl_0KB_p_bwbp512" "saga_vcl_0KB_p_bwbp64" "saga_vcl_0KB_p_NE" "saga_vcl_0KB_s_bwbp128" "saga_vcl_0KB_s_bwbp16" "saga_vcl_0KB_s_bwbp256" "saga_vcl_0KB_s_bwbp32" "saga_vcl_0KB_s_bwbp512" "saga_vcl_0KB_s_bwbp64" "saga_vcl_0KB_s_NE")
binaries2KB=("champsim32k" "champsim64k" "saga_vcl_2KB_b_bwbp128" "saga_vcl_2KB_b_bwbp16" "saga_vcl_2KB_b_bwbp256" "saga_vcl_2KB_b_bwbp32" "saga_vcl_2KB_b_bwbp512" "saga_vcl_2KB_b_bwbp64" "saga_vcl_2KB_b_NE" "saga_vcl_2KB_p_bwbp128" "saga_vcl_2KB_p_bwbp16" "saga_vcl_2KB_p_bwbp256" "saga_vcl_2KB_p_bwbp32" "saga_vcl_2KB_p_bwbp512" "saga_vcl_2KB_p_bwbp64" "saga_vcl_2KB_p_NE" "saga_vcl_2KB_s_bwbp128" "saga_vcl_2KB_s_bwbp16" "saga_vcl_2KB_s_bwbp256" "saga_vcl_2KB_s_bwbp32" "saga_vcl_2KB_s_bwbp512" "saga_vcl_2KB_s_bwbp64" "saga_vcl_2KB_s_NE")
binaries4KB=("champsim32k" "champsim64k" "saga_vcl_4KB_b_bwbp128" "saga_vcl_4KB_b_bwbp16" "saga_vcl_4KB_b_bwbp256" "saga_vcl_4KB_b_bwbp32" "saga_vcl_4KB_b_bwbp512" "saga_vcl_4KB_b_bwbp64" "saga_vcl_4KB_b_NE" "saga_vcl_4KB_p_bwbp128" "saga_vcl_4KB_p_bwbp16" "saga_vcl_4KB_p_bwbp256" "saga_vcl_4KB_p_bwbp32" "saga_vcl_4KB_p_bwbp512" "saga_vcl_4KB_p_bwbp64" "saga_vcl_4KB_p_NE" "saga_vcl_4KB_s_bwbp128" "saga_vcl_4KB_s_bwbp16" "saga_vcl_4KB_s_bwbp256" "saga_vcl_4KB_s_bwbp32" "saga_vcl_4KB_s_bwbp512" "saga_vcl_4KB_s_bwbp64" "saga_vcl_4KB_s_NE")
binaries8KB=("champsim32k" "champsim64k" "saga_vcl_8KB_b_bwbp128" "saga_vcl_8KB_b_bwbp16" "saga_vcl_8KB_b_bwbp256" "saga_vcl_8KB_b_bwbp32" "saga_vcl_8KB_b_bwbp512" "saga_vcl_8KB_b_bwbp64" "saga_vcl_8KB_b_NE" "saga_vcl_8KB_p_bwbp128" "saga_vcl_8KB_p_bwbp16" "saga_vcl_8KB_p_bwbp256" "saga_vcl_8KB_p_bwbp32" "saga_vcl_8KB_p_bwbp512" "saga_vcl_8KB_p_bwbp64" "saga_vcl_8KB_p_NE" "saga_vcl_8KB_s_bwbp128" "saga_vcl_8KB_s_bwbp16" "saga_vcl_8KB_s_bwbp256" "saga_vcl_8KB_s_bwbp32" "saga_vcl_8KB_s_bwbp512" "saga_vcl_8KB_s_bwbp64" "saga_vcl_8KB_s_NE")
python /cluster/home/romankb/workspace/ChampSim/data_collector.py --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/champsim_vcl --traces_directory /cluster/work/users/romankb/dataset/tanvir --output_dir /cluster/work/users/romankb/results/champsim_vcl_large/ >> ~/pyrunner_vcl.log

for binary in ${binaries0KB[@]}
do
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 5000000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/0KB/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/spec --nosub --output_dir /cluster/work/users/romankb/results/new_traces_grid/0KB/sizes_${binary}/ >> ~/pyrunner_ipc1_spec_${binary}.log & # We got spec in dpc3
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 5000000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/0KB/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/server --nosub --output_dir /cluster/work/users/romankb/results/new_traces_grid/0KB/sizes_${binary}/ >> ~/pyrunner_ipc1_server_${binary}.log & # We got spec in dpc3
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 5000000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/0KB/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/client --nosub --output_dir /cluster/work/users/romankb/results/new_traces_grid/0KB/sizes_${binary}/ >> ~/pyrunner_ipc1_client_${binary}.log & # We got spec in dpc3
done

for binary in ${binaries2KB[@]}
do
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 5000000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/2KB/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/spec --nosub --output_dir /cluster/work/users/romankb/results/new_traces_grid/2KB/sizes_${binary}/ >> ~/pyrunner_ipc1_spec_${binary}.log &
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 5000000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/2KB/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/server --nosub --output_dir /cluster/work/users/romankb/results/new_traces_grid/2KB/sizes_${binary}/ >> ~/pyrunner_ipc1_server_${binary}.log &
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 5000000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/2KB/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/client --nosub --output_dir /cluster/work/users/romankb/results/new_traces_grid/2KB/sizes_${binary}/ >> ~/pyrunner_ipc1_client_${binary}.log &
done


for binary in ${binaries4KB[@]}
do
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 5000000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/4KB/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/spec --nosub --output_dir /cluster/work/users/romankb/results/new_traces_grid/4KB/sizes_${binary}/ >> ~/pyrunner_ipc1_spec_${binary}.log &
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 5000000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/4KB/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/server --nosub --output_dir /cluster/work/users/romankb/results/new_traces_grid/4KB/sizes_${binary}/ >> ~/pyrunner_ipc1_server_${binary}.log &
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 5000000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/4KB/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/client --nosub --output_dir /cluster/work/users/romankb/results/new_traces_grid/4KB/sizes_${binary}/ >> ~/pyrunner_ipc1_client_${binary}.log &
done

for binary in ${binaries8KB[@]}
do
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 5000000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/8KB/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/spec --nosub --output_dir /cluster/work/users/romankb/results/new_traces_grid/8KB/sizes_${binary}/ >> ~/pyrunner_ipc1_spec_${binary}.log &
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 5000000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/8KB/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/server --nosub --output_dir /cluster/work/users/romankb/results/new_traces_grid/8KB/sizes_${binary}/ >> ~/pyrunner_ipc1_server_${binary}.log &
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 5000000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/8KB/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1_new/client --nosub --output_dir /cluster/work/users/romankb/results/new_traces_grid/8KB/sizes_${binary}/ >> ~/pyrunner_ipc1_client_${binary}.log &
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

