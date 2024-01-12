#!/bin/bash
#SBATCH --job-name="build_experiment_binaries"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c8
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00-01:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/users/romankb/slurm-build-%j.err
#SBATCH -o /cluster/work/users/romankb/slurm-build-%j.out

module load Boost/1.79.0-GCC-11.3.0
~/workspace/ChampSim/saga_grid_build.sh
