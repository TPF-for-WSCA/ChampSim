#!/bin/bash
#SBATCH --job-name="num-collection-champsim-quick"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c8
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00-12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/users/romankb/slurm-%j.err
#SBATCH -o /cluster/work/users/romankb/slurm-%j.out

module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0
#binaries=("champsim32k" "champsim64k" "champsim_vcl_buffer_16" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d" "champsim128m")
binaries=("champsim_data_16k" "champsim_data_32k" "champsim_data_64k" "champsim_data_128k" "champsim_vcl_buffer_64_f_data" "champsim_vcl_buffer_64_lru_data" "champsim_vcl_buffer_64_lru_2w_data" "champsim_vcl_buffer_64_lru_4w_data" "champsim_vcl_buffer_64_lru_8w_data" "champsim_vcl_buffer_64d_data" "champsim_vcl_buffer_128_f_data" "champsim_vcl_buffer_128_lru_data" "champsim_vcl_buffer_128_lru_2w_data" "champsim_vcl_buffer_128_lru_4w_data" "champsim_vcl_buffer_128_lru_8w_data" "champsim_vcl_buffer_128d_data" "champsim_simple_vcl_buffer_64_lru_8w")
for binary in ${binaries[@]}
do
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --instructions 90000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/${binary} --trace_format p --traces_directory /cluster/work/users/romankb/dataset/tanvir --output_dir /cluster/work/users/romankb/results/full_analysis_v2/tanvir/sizes_${binary}/ >> ~/pyrunner_${binary}.log &
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

