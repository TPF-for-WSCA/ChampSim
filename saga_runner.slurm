#!/bin/bash
#SBATCH --job-name="num-collection-experiment-extended"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c20
#SBATCH --mem-per-cpu=4G
#SBATCH --time=00-16:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/users/romankb/slurm-%j.err
#SBATCH -o /cluster/work/users/romankb/slurm-%j.outi

module load Boost/1.77.0-GCC-11.2.0
module load Python/3.9.6-GCCcore-11.2.0
python /cluster/home/romankb/workspace/ChampSim/data_collector.py --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/champsim --traces_directory /cluster/work/users/romankb/dataset/tanvir --output_dir /cluster/work/users/romankb/results/champsim/

