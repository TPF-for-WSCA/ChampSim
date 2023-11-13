#!/bin/bash
#SBATCH --job-name="storage_efficiency_calculation"
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH -c12
#SBATCH --mem-per-cpu=8G
#SBATCH --time=12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/romankb/storage-efficicency-%j.err
#SBATCH -o /cluster/work/romankb/storage-efficicency-%j.out
module load Boost/1.77.0-GCC-11.2.0
module load Python/3.9.6-GCCcore-11.2.0
benchmarks=("ipc1_server" "ipc1_client" "ipc1_spec")
baseline_configs=("sizes_champsim32k")
vcl_16_configs=("sizes_champsim_vcl_buffer_fdip_16way_64d")
vcl_18_configs=("sizes_champsim_vcl_buffer_fdip_64d")


old_dir=pwd

for benchmark in ${benchmarks[@]}
do
    echo "Handling ${benchmark}"
    for config in ${baseline_configs[@]}
    do
        echo "\tHandling ${config}"
        python ~/ChampSim/aggregation_scripts/apply_merge_strategies.py ./${benchmark}/${config} storage_efficiency arm &
    done
    for config in ${vcl_18_configs[@]}
    do
        echo "\tHandling ${config}"
        python ~/ChampSim/aggregation_scripts/apply_merge_strategies.py ./${benchmark}/${config} storage_efficiency arm --vcl-configuration 4 4 4 4 8 8 8 8 12 16 20 32 36 36 48 64 64 64 &
    done
    for config in ${vcl_16_configs[@]}
    do
        echo "\tHandling ${config}"
        python ~/ChampSim/aggregation_scripts/apply_merge_strategies.py ./${benchmark}/${config} storage_efficiency arm --vcl-configuration 4 4 8 8 12 12 20 24 32 36 40 48 64 64 64 &
    done
done

for job in `jobs -p`
do
    echo $job
    wait $job
done



echo "DONE"