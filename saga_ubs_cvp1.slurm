#!/bin/bash
#SBATCH --job-name="num-collection-champsim-grid"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c8
#SBATCH --mem-per-cpu=20G
#SBATCH --time=01-00:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/users/romankb/cvp1-%j.err
#SBATCH -o /cluster/work/users/romankb/cvp1-%j.out

module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0

binaries0KB=("champsim32k" "champsim64k" "ubs")

for binary in ${binaries0KB[@]}
do
    python /cluster/home/romankb/workspace/ChampSim/data_collector.py --warmup 50000000 --evaluation 50000000 --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/${binary} --traces_directory /cluster/work/users/romankb/dataset/cvp1 --nosub --output_dir /cluster/work/users/romankb/results/isca_cvp1_fixed/raw_logs/sizes_${binary}/ >> ~/pyrunner_cvp1_${binary}.log & # We got spec in dpc3
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

