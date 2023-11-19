#!/bin/bash
#SBATCH --job-name="num-collection-champsim-sizes"
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH -c8
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00-12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/romankb/slurm-default-sizes-%j.err
#SBATCH -o /cluster/work/romankb/slurm-default-sizes-%j.out

echo "Remembered to adjust the number of ways in the config?"

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"/cluster/home/romankb/xed/kits/xed-install-base-2023-04-21-lin-x86-64/lib"
module load Boost/1.79.0-GCC-11.3.0
module load Python/3.9.6-GCCcore-11.2.0
binaries=("champsim_perfect_predictor")
for binary in ${binaries[@]}
do
    python /cluster/home/romankb/ChampSim/data_collector.py --instructions 90000000 --experiment_executable /cluster/home/romankb/ChampSim/bin/${binary} --trace_format p --traces_directory /cluster/work/romankb/dataset/tanvir --output_dir /cluster/work/romankb/results/perfect_predictor/tanvir/sizes_${binary}/ >> ~/pyrunner_perfect_predictor_${binary}.log & 
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

