#!/bin/bash
#SBATCH --job-name="build_experiment_binaries"
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH -c16
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00-00:15:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=NONE
#SBATCH -e /cluster/work/romankb/slurm-build-%j.err
#SBATCH -o /cluster/work/romankb/slurm-build-%j.out

module load Boost/1.79.0-GCC-11.3.0
# ~/ChampSim/idun_build.sh
~/ChampSim/idun_grid_build.shs
