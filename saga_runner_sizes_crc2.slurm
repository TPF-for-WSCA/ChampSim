#!/bin/bash
#SBATCH --job-name="num-collection-champsim-quick"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c16
#SBATCH --mem-per-cpu=8G
#SBATCH --time=02-00:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/users/romankb/slurm-%j.err
#SBATCH -o /cluster/work/users/romankb/slurm-%j.out

module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0
#binaries=("champsim32k" "champsim64k" "champsim_vcl_buffer_16" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d" "champsim128m")
#binaries=("champsim32k" "champsim64k" "champsim128m" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d" "champsim_vcl_buffer_16")
binaries=("champsim32k" "champsim64k" "champsim128m" "champsim_vcl_buffer_64d_max_way")
for binary in ${binaries[@]}
do
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --instructions 200000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/crc2/cloud --nosub --intel --output_dir /cluster/work/users/romankb/results/crosscheck/crc2_cloud/sizes_${binary}/ >> /cluster/work/users/romankb/logs_crc2_cloud_${binary}.log
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --instructions 200000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/crc2/spec --nosub --intel --output_dir /cluster/work/users/romankb/results/crosscheck/crc2_spec/sizes_${binary}/ >> /cluster/work/users/romankb/logs_crc2_spec_${binary}.log
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

