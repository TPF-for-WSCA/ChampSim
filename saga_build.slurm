#!/bin/bash
#SBATCH --job-name="build_experiment_binaries"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c20
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00-00:10:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/users/romankb/slurm-build-%j.err
#SBATCH -o /cluster/work/users/romankb/slurm-build-%j.out

module load Boost/1.79.0-GCC-11.3.0
#/cluster/projects/nn4650k/workspace/ChampSim/saga_build.sh
/cluster/projects/nn4650k/workspace/ChampSim/saga_directory_build.sh
#/cluster/projects/nn4650k/workspace/ChampSim/saga_size_sensitivity_build.sh
#/cluster/projects/nn4650k/workspace/ChampSim/saga_grid_build.sh
#/cluster/projects/nn4650k/workspace/ChampSim/saga_btb_build.sh
