#!/bin/bash
#SBATCH --job-name="build_experiment_binaries"
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH -c8
#SBATCH --mem-per-cpu=2G
#SBATCH --time=00-00:15:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/romankb/slurm-build-%j.err
#SBATCH -o /cluster/work/romankb/slurm-build-%j.out

module load Boost/1.77.0-GCC-11.2.0
~/ChampSim/idun_build.sh
