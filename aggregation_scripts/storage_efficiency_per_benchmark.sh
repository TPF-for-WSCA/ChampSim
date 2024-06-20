#!/bin/bash
#SBATCH --job-name="storage_efficiency_calculation"
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH -c12
#SBATCH --mem-per-cpu=8G
#SBATCH --time=12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=NONE
#SBATCH -e /cluster/work/romankb/storage-efficicency-%j.err
#SBATCH -o /cluster/work/romankb/storage-efficicency-%j.out
module load Boost/1.79.0-GCC-11.3.0
module load Python/3.10.4-GCCcore-11.3.0
benchmarks=("ipc1_server" "ipc1_client" "ipc1_spec")
#benchmarks=("whiskey" "merced" "delta" "charlie")
#baseline_configs=("sizes_champsim32k")
baseline_configs=()
vcl_16_configs=("sizes_champsim_16B")
vcl_32_configs=("sizes_champsim_32B")
#vcl_18_configs=("sizes_ubs")
vcl_18_configs=()


old_dir=pwd

for benchmark in ${benchmarks[@]}
do
    echo "Handling ${benchmark}"
    for config in ${baseline_configs[@]}
    do
        echo "\tHandling ${config}"
        python /cluster/projects/nn4650k/workspace/ChampSim/aggregation_scripts/apply_merge_strategies.py ./${benchmark}/${config} storage_efficiency arm &
    done
    for config in ${vcl_18_configs[@]}
    do
        echo "\tHandling ${config}"
        python /cluster/projects/nn4650k/workspace/ChampSim/aggregation_scripts/apply_merge_strategies.py ./${benchmark}/${config} storage_efficiency arm --vcl-configuration 4 4 4 4 8 8 8 8 12 16 20 32 36 36 48 64 64 64 &
    done
    for config in ${vcl_16_configs[@]}
    do
        echo "\tHandling ${config}"
        python /cluster/projects/nn4650k/workspace/ChampSim/aggregation_scripts/apply_merge_strategies.py ./${benchmark}/${config} storage_efficiency arm --vcl-configuration 16 16 16 16 16 16 16 16 16 16 16 16 16 16 16 &
    done
    for config in ${vcl_32_configs[@]}
    do
        echo "\tHandling ${config}"
        python /cluster/projects/nn4650k/workspace/ChampSim/aggregation_scripts/apply_merge_strategies.py ./${benchmark}/${config} storage_efficiency arm --vcl-configuration 32 32 32 32 32 32 32 32 32 32 32 32 32 32 32 32 32 &
    done
done

for job in `jobs -p`
do
    echo $job
    wait $job
done



echo "DONE"