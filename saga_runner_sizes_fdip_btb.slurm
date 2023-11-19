#!/bin/bash
#SBATCH --job-name="num-collection-champsim-quick"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c16
#SBATCH --mem-per-cpu=4G
#SBATCH --time=00-03:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/users/romankb/slurm-%j.err
#SBATCH -o /cluster/work/users/romankb/slurm-%j.out

module load Boost/1.79.0-GCC-11.3.0
module load Python/3.9.6-GCCcore-11.2.0
binaries=("champsim32k" "champsim32k_fdip" "champsim32k_fdip_btb2048" "champsim32k_fdip_btb8192" "champsim_vcl_buffer_fdip_64d" "champsim_vcl_buffer_fdip_btb2048_64d" "champsim_vcl_buffer_fdip_btb8192_64d" "champsim_vcl_buffer_64d") 
#binaries=("champsim32k" "champsim64k" "champsim_simple_vcl_buffer_64_lru_8w")

for binary in ${binaries[@]}
do
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1/spec --nosub --output_dir /cluster/work/users/romankb/results/fdip_btb_analysis1/ipc1_spec/sizes_${binary}/ >> ~/pyrunner_ipc1_spec_${binary}.log & # We got spec in dpc3
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1/server --nosub --output_dir /cluster/work/users/romankb/results/fdip_btb_analysis1/ipc1_server/sizes_${binary}/ >> ~/pyrunner_ipc1_server_${binary}.log &
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1/client --nosub --output_dir /cluster/work/users/romankb/results/fdip_btb_analysis1/ipc1_client/sizes_${binary}/ >> ~/pyrunner_ipc1_client_${binary}.log &
done

for job in `jobs -p`
do
    echo $job
    wait $job
done