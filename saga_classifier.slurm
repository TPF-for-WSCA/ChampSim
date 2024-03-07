#!/bin/bash
#SBATCH --job-name="offset_classifier"
#SBATCH --account=nn4650k
#SBATCH --nodes=1
#SBATCH -c8
#SBATCH --mem-per-cpu=22G
#SBATCH --time=02-00:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/users/romankb/classifier.err
#SBATCH -o /cluster/work/users/romankb/classifier.out

module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0
module load scikit-learn/1.1.2-foss-2022a

groups=("ipc1_client" "ipc1_server" "ipc1_spec")

for g in ${groups[@]}
do
    python /cluster/projects/nn4650k/workspace/ChampSim/hash_matcher/classifier_approach.py /cluster/work/users/romankb/results/btb_offset_data/${g}/complete_pc_offset_mapping.tsv > /cluster/work/users/romankb/results/btb_offset_data/${g}/classifier_performance_fast.txt &
done


for job in `jobs -p`
do
    echo $job
    wait $job
done
