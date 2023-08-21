#!/bin/bash
#SBATCH --job-name="num-collection-champsim-sizes"
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH -c16
#SBATCH --mem-per-cpu=8G
#SBATCH --time=12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/romankb/slurm-default-sizes-%j.err
#SBATCH -o /cluster/work/romankb/slurm-default-sizes-%j.out
module load Boost/1.77.0-GCC-11.2.0
module load Python/3.9.6-GCCcore-11.2.0

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"/cluster/home/romankb/xed/kits/xed-install-base-2023-04-21-lin-x86-64/lib"
module load Boost/1.77.0-GCC-11.2.0
module load Python/3.9.6-GCCcore-11.2.0
binaries=("champsim_data_32k" "champsim_data_64k" "champsim_vcl_buffer_64d_data" "champsim_vcl_buffer_64_lru_8w" "champsim_vcl_buffer_64_3lip__lru_8w" "champsim_vcl_buffer_64_lip__lru_8w" "champsim_vcl_buffer_64_4lip__lru_8w")
#binaries=("champsim_vcl_buffer_64d_data_small")


#binaries=("champsim_data_16k" "champsim_data_32k" "champsim_data_64k" "champsim_data_128k" "champsim_vcl_buffer_64_f_data" "champsim_vcl_buffer_64_lru_data" "champsim_vcl_buffer_64_lru_2w_data" "champsim_vcl_buffer_64_lru_4w_data" "champsim_vcl_buffer_64_lru_8w_data" "champsim_vcl_buffer_64d_data" "champsim_vcl_buffer_128_f_data" "champsim_vcl_buffer_128_lru_data" "champsim_vcl_buffer_128_lru_2w_data" "champsim_vcl_buffer_128_lru_4w_data" "champsim_vcl_buffer_128_lru_8w_data" "champsim_vcl_buffer_128d_data" "champsim_simple_vcl_buffer_64_lru_8w_data")
for binary in ${binaries[@]}
do
    python /cluster/home/romankb/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/home/romankb/ChampSim/bin/${binary} --traces_directory /cluster/work/romankb/dataset/ipc1/spec --nosub --output_dir /cluster/work/romankb/results/full_analysis_v13/ipc1_spec/sizes_${binary}/ >> ~/pyrunner_ipc1_spec_${binary}.log & # We got spec in dpc3
    python /cluster/home/romankb/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/home/romankb/ChampSim/bin/${binary} --traces_directory /cluster/work/romankb/dataset/ipc1/server --nosub --output_dir /cluster/work/romankb/results/full_analysis_v13/ipc1_server/sizes_${binary}/ >> ~/pyrunner_ipc1_server_${binary}.log &
    python /cluster/home/romankb/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/home/romankb/ChampSim/bin/${binary} --traces_directory /cluster/work/romankb/dataset/ipc1/client --nosub --output_dir /cluster/work/romankb/results/full_analysis_v13/ipc1_client/sizes_${binary}/ >> ~/pyrunner_ipc1_client_${binary}.log &
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

