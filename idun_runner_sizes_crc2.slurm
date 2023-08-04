#!/bin/bash
#SBATCH --job-name="num-collection-champsim-sizes"
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH -c16
#SBATCH --mem-per-cpu=8G
#SBATCH --time=01-12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/romankb/slurm-default-sizes-%j.err
#SBATCH -o /cluster/work/romankb/slurm-default-sizes-%j.out

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"/cluster/home/romankb/xed/kits/xed-install-base-2023-04-21-lin-x86-64/lib"
module load Boost/1.77.0-GCC-11.2.0
module load Python/3.9.6-GCCcore-11.2.0
binaries=("champsim_data_16k" "champsim_data_32k" "champsim_data_64k" "champsim_data_128k" "champsim_vcl_buffer_64d_data" "champsim_vcl_buffer_64_lru_2w_data" "champsim_vcl_buffer_64_lru_4w_data" "champsim_vcl_buffer_64_lru_8w_data" "champsim_vcl_buffer_64_3lip_d_data" "champsim_vcl_buffer_64_4lip_d_data" "champsim_vcl_buffer_64_lip_d_data" "champsim_vcl_buffer_64_2lip_d_data" "champsim_vcl_buffer_64_3lip__lru_8w_data" "champsim_vcl_buffer_64_4lip__lru_8w_data" "champsim_vcl_buffer_64_lip__lru_8w_data" "champsim_vcl_buffer_64_2lip__lru_8w_data")
for binary in ${binaries[@]}
do
    python /cluster/home/romankb/ChampSim/data_collector.py --warmup 1000000 --evaluation 10000000 --experiment_executable /cluster/home/romankb/ChampSim/bin/${binary} --traces_directory /cluster/work/romankb/dataset/crc2/cloud --nosub --output_dir /cluster/work/romankb/results/full_analysis_v9/crc2_cloud/sizes_${binary}/ >> ~/pyrunner_crc2_cloud_${binary}.log
    python /cluster/home/romankb/ChampSim/data_collector.py --warmup 1000000 --evaluation 10000000 --experiment_executable /cluster/home/romankb/ChampSim/bin/${binary} --traces_directory /cluster/work/romankb/dataset/crc2/spec --nosub --intel --output_dir /cluster/work/romankb/results/full_analysis_v9/crc2_spec/sizes_${binary}/ >> ~/pyrunner_crc2_spec_${binary}.log # WE GOT SPEC IN DPC3
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

