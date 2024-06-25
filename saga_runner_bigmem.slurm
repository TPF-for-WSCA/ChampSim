#!/bin/bash
#SBATCH --job-name="num-collection-champsim-bigmem"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c8
#SBATCH --mem-per-cpu=2G
#SBATCH --time=03-00:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=NONE
#SBATCH -e /cluster/work/users/romankb/slurm-%j.err
#SBATCH -o /cluster/work/users/romankb/slurm-%j.out

module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0
python /cluster/home/romankb/workspace/ChampSim/data_collector.py --experiment_executable /cluster/home/romankb/workspace/ChampSim/bin/champsim_bigcache --traces_directory /cluster/work/users/romankb/dataset/tanvir --output_dir /cluster/work/users/romankb/results/champsim_bigcache_fixed/ >> ~/pyrunner.log

