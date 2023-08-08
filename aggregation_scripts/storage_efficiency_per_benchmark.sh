#!/bin/bash
#SBATCH --job-name="storage_efficiency_calculation"
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH -c12
#SBATCH --mem-per-cpu=8G
#SBATCH --time=12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e ~/storage-efficicency-%j.err
#SBATCH -o ~/storage-efficicency-%j.out

benchmarks=("ipc1_server" "ipc1_client" "ipc1_spec")
baseline_configs=("sizes_champsim_data_32k" "sizes_champsim_data_64k")
vcl_configs=("sizes_champsim_vcl_buffer_64d_data")

chroot=""

if [ -z ${champsim_root+x} ]; then chroot=""; else chroot=$champsim_root; fi

echo ${chroot}

old_dir=pwd

for benchmark in ${benchmarks[@]}
do
    echo "Handling ${benchmark}"
    for config in ${baseline_configs[@]}
    do
        echo "\tHandling ${config}"
    	python ${chroot}/ChampSim/aggregation_scripts/apply_merge_strategies.py ./${benchmark}/${config} storage_efficiency arm &
    done
    for config in ${vcl_configs[@]}
    do
        echo "\tHandling ${config}"
    	python ${chroot}/ChampSim/aggregation_scripts/apply_merge_strategies.py ./${benchmark}/${config} storage_efficiency arm --vcl-configuration 4 8 12 16 16 16 20 28 36 44 48 64 64 64 &
    done
done

for job in `jobs -p`
do
    echo $job
    wait $job
done



echo "DONE"