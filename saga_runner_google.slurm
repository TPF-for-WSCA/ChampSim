#!/bin/bash
#SBATCH --job-name="num-collection-champsim-quick"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c16
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00-24:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=NONE
#SBATCH -e /cluster/work/users/romankb/slurm-%j.err
#SBATCH -o /cluster/work/users/romankb/slurm-%j.out

module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0
#binaries=("champsim64k" "champsim32k" "champsim128m" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d_arm" "champsim_vcl_buffer_16")
#binaries=("champsim64k" "champsim32k" "champsim128m" "champsim_vcl_buffer_16_a" "champsim_vcl_buffer_64d_arm" "champsim_vcl_buffer_16")
binaries=("champsim32k" "champsim64k" "ubs_overhead_isca")
for binary in ${binaries[@]}
do
    srun --account=nn4650k -J num-collection-${binary} --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t01:00:00 -o /cluster/work/users/romankb/google-%j.out -e /cluster/work/users/romankb/google-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --intel --experiment_executable /cluster/projects/nn4650k/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/google/whiskey --nosub --output_dir /cluster/work/users/romankb/results/google_micro/whiskey/sizes_${binary}/ >> ~/pyrunner_google_whiskey${binary}.log &
    srun --account=nn4650k -J num-collection-${binary} --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t01:00:00 -o /cluster/work/users/romankb/google-%j.out -e /cluster/work/users/romankb/google-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --intel --experiment_executable /cluster/projects/nn4650k/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/google/merced --nosub --output_dir /cluster/work/users/romankb/results/google_micro/merced/sizes_${binary}/ >> ~/pyrunner_google_merced${binary}.log  &
    srun --account=nn4650k -J num-collection-${binary} --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t01:00:00 -o /cluster/work/users/romankb/google-%j.out -e /cluster/work/users/romankb/google-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --intel --experiment_executable /cluster/projects/nn4650k/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/google/delta --nosub --output_dir /cluster/work/users/romankb/results/google_micro/delta/sizes_${binary}/ >> ~/pyrunner_google_delta${binary}.log &
    srun --account=nn4650k -J num-collection-${binary} --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=8G -n1 -c20 -t01:00:00 -o /cluster/work/users/romankb/google-%j.out -e /cluster/work/users/romankb/google-%j.err python /cluster/projects/nn4650k/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --intel --experiment_executable /cluster/projects/nn4650k/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/google/charlie --nosub --output_dir /cluster/work/users/romankb/results/google_micro/charlie/sizes_${binary}/ >> ~/pyrunner_google_charlie${binary}.log &
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

