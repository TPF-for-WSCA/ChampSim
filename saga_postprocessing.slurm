#!/bin/bash
#SBATCH --job-name="postprocessing"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c8
#SBATCH --mem-per-cpu=8G
#SBATCH --time=01-12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/users/romankb/slurm-postproc-%j.err
#SBATCH -o /cluster/work/users/romankb/slurm-postproc-%j.out

module load Boost/1.77.0-GCC-11.2.0
module load Python/3.9.6-GCCcore-11.2.0
directories=("champsim8k" "champsim64k" "champsim4k" "champsim32k" "champsim2k" "champsim1k" "champsim16k" "champsim128k")
for directory in ${directories[@]}
do
    python /cluster/home/romankb/workspace/ChampSimISCA/aggregation_scripts/apply_merge_strategies.py /cluster/work/users/romankb/results/champsim_${directory}/ >> ~/pyaggregate_smallcache_${directory}.log &
done

for job in `jobs -p`
do
    echo $job
    wait $job
done

