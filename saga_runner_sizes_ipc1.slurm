#!/bin/bash
#SBATCH --job-name="num-collection-champsim-quick"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c16
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00-12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/users/romankb/slurm-%j.err
#SBATCH -o /cluster/work/users/romankb/slurm-%j.out

module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0
#binaries=("champsim64k" "champsim32k" "champsim128m" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d_arm" "champsim_vcl_buffer_16")
#binaries=("champsim64k" "champsim32k" "champsim128m" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d_arm" "champsim_vcl_buffer_16")
binaries=("champsim_data_16k" "champsim_data_32k" "champsim_data_64k" "champsim_data_128k" "champsim_vcl_buffer_64_f_data" "champsim_vcl_buffer_64_lru_data" "champsim_vcl_buffer_64_lru_2w_data" "champsim_vcl_buffer_64_lru_4w_data" "champsim_vcl_buffer_64_lru_8w_data" "champsim_vcl_buffer_64d_data" "champsim_vcl_buffer_128_f_data" "champsim_vcl_buffer_128_lru_data" "champsim_vcl_buffer_128_lru_2w_data" "champsim_vcl_buffer_128_lru_4w_data" "champsim_vcl_buffer_128_lru_8w_data" "champsim_vcl_buffer_128d_data" "champsim_simple_vcl_buffer_64_lru_8w")
for binary in ${binaries[@]}
do
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --instructions 100000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1/spec --nosub --output_dir /cluster/work/users/romankb/results/full_analysis_v2/ipc1_spec/sizes_${binary}/ >> ~/pyrunner_ipc1_spec${binary}.log 
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --instructions 100000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1/server --nosub --output_dir /cluster/work/users/romankb/results/full_analysis_v2/ipc1_server/sizes_${binary}/ >> ~/pyrunner_ipc1_server${binary}.log 
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --instructions 100000000 --/crosscheckexperiment_executable /cluster/home/romankb/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1/client --nosub --output_dir /cluster/work/users/romankb/results/full_analysis_v2/ipc1_client/sizes_${binary}/ >> ~/pyrunner_ipc1_client${binary}.log 
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

