#!/bin/bash
#SBATCH --job-name="num-collection-champsim-quick"
#SBATCH --account=nn4650k
#SBATCH --nodes=4
#SBATCH -c16
#SBATCH --mem-per-cpu=4G
#SBATCH --time=01-00:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/users/romankb/slurm-%j.err
#SBATCH -o /cluster/work/users/romankb/slurm-%j.out

module load Boost/1.77.0-GCC-11.2.0
module load Python/3.9.6-GCCcore-11.2.0
binaries=("champsim64k" "champsim32k" "champsim128m" "champsim_vcl_buffer_16")
for binary in ${binaries[@]}
do
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --instructions 100000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/ipc1 --nosub --output_dir /cluster/work/users/romankb/results/ipc1/sizes_${binary}/ >> ~/pyrunner_${binary}.log & 
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

