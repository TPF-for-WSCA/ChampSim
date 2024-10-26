#!/bin/bash
#SBATCH --job-name="extract_ideal_way_size"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c6
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00-12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/users/romankb/slurm-idealway-%j.err
#SBATCH -o /cluster/work/users/romankb/slurm-idealway-%j.out

module load Python/3.10.4-GCCcore-11.3.0
python ~/workspace/ChampSim/aggregation_scripts/apply_merge_strategies.py /cluster/work/users/romankb/results/ipc1_spec/sizes_champsim64k optimal_way &
python ~/workspace/ChampSim/aggregation_scripts/apply_merge_strategies.py /cluster/work/users/romankb/results/ipc1_server/sizes_champsim64k optimal_way &
python ~/workspace/ChampSim/aggregation_scripts/apply_merge_strategies.py /cluster/work/users/romankb/results/ipc1_client/sizes_champsim64k optimal_way &
python ~/workspace/ChampSim/aggregation_scripts/apply_merge_strategies.py /cluster/work/users/romankb/results/crc2_cloude/sizes_champsim64k optimal_way &
python ~/workspace/ChampSim/aggregation_scripts/apply_merge_strategies.py /cluster/work/users/romankb/results/crc2_spec/sizes_champsim64k optimal_way &
python ~/workspace/ChampSim/aggregation_scripts/apply_merge_strategies.py /cluster/work/users/romankb/results/tanvir/sizes_champsim64k optimal_way &

for job in `jobs -p`
do
    echo $job
    wait $job
done
