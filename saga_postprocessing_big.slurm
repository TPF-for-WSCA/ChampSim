#!/bin/bash
#SBATCH --job-name="postprocessing"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=01-12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/users/romankb/slurm-postproc-%j.err
#SBATCH -o /cluster/work/users/romankb/slurm-postproc-%j.out

module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0
python /cluster/home/romankb/workspace/ChampSim/aggregation_scripts/apply_merge_strategies.py /cluster/work/users/romankb/results/champsim_bigcache_fixed/ >> ~/pyaggregate_smallcache.log

