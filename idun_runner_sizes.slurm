#!/bin/bash
#SBATCH --job-name="num-collection-champsim-sizes"
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH -c8
#SBATCH --mem-per-cpu=2G
#SBATCH --time=00-06:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/romankb/slurm-default-sizes-%j.err
#SBATCH -o /cluster/work/romankb/slurm-default-sizes-%j.out

module load Boost/1.77.0-GCC-11.2.0
module load Python/3.9.6-GCCcore-11.2.0
binaries=("champsim8k" "champsim64k" "champsim4k" "champsim32k" "champsim2k" "champsim1k" "champsim16k" "champsim128k" "champsim_bigcache")
for binary in ${binaries[@]}
do
    python /cluster/home/romankb/ChampSim/data_collector.py --instructions 90000000 --experiment_executable /cluster/home/romankb/ChampSim/bin/${binary} --traces_directory /cluster/work/romankb/dataset/tanvir --output_dir /cluster/work/romankb/results/sizes_${binary}/ >> ~/pyrunner_${binary}.log & 
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

