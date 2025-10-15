#!/bin/bash
#SBATCH --job-name="num-collection-champsim-sizes"
#SBATCH --account=share-ie-idi
#SBATCH --nodes=1
#SBATCH -c16
#SBATCH --mem_per_cpu=8G
#SBATCH --time=01-12:00:00
#SBATCH --mail-user=romankb@ntnu.no
#SBATCH --mail-type=ALL
#SBATCH -e /cluster/work/romankb/slurm-default-sizes-%j.err
#SBATCH -o /cluster/work/romankb/slurm-default-sizes-%j.out

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:"/cluster/home/romankb/xed/kits/xed-install-base-2023-04-21-lin-x86-64/lib"

module load Python/3.10.8-GCCcore-12.2.0
module load GCC/12.2.0
module load GCCcore/12.2.0

suffix="ittage_cvp_dpc3"
binary_dir=("btb_full_grid_search") # "btb_size_region_sensitivity" "btb_4k_10b_tag_sensitivity" "btb_4k_12b_tag_sensitivity" )
count=11
timelimit="8:00:00"
warmup=50000000
simulation=50000000

mem_per_cpu="20G"
max_core_count=8

for dir in ${binary_dir[@]}
do
    echo $dir
    for bin in ~/ChampSim/bin/$dir/*
    do
        echo $bin
        # IPC-1 Benchmarks
        # srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=${mem_per_cpu} -n1 -c$((max_core_count / 2)) -t$timelimit -o /cluster/work/romankb/latency-spec-${dir}-$(basename ${bin})-%j.out    -e /cluster/work/romankb/latency-spec-${dir}-$(basename ${bin})-%j.err    python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/spec   --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}${suffix:+_$suffix}/ipc1_spec/sizes_$(basename ${bin})/              &>> /cluster/work/romankb/pyrunner_latency_fixed_ipc1_spec_$(basename ${bin}).log &
        # srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=${mem_per_cpu} -n1 -c$((max_core_count / 2)) -t$timelimit -o /cluster/work/romankb/latency-client-${dir}-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-client-${dir}-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/client --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}${suffix:+_$suffix}/ipc1_client/sizes_$(basename ${bin})/            &>> /cluster/work/romankb/pyrunner_latency_fixed_ipc1_client_$(basename ${bin}).log &
        # srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=${mem_per_cpu} -n1 -c${max_core_count} -t$timelimit -o /cluster/work/romankb/latency-server-${dir}-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-server-${dir}-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --traces_directory /cluster/work/romankb/dataset/IPC1_new_translated/server --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}${suffix:+_$suffix}/ipc1_server/sizes_$(basename ${bin})/            &>> /cluster/work/romankb/pyrunner_latency_fixed_ipc1_server_$(basename ${bin}).log &
        # SPECCPU / DPC-3 Benchmarks # Deactivated for now - most have little impact, biggest impact however also here
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=${mem_per_cpu} -n1 -c${max_core_count} -t$timelimit -o /cluster/work/romankb/latency-server-${dir}-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-server-${dir}-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/dpc3                       --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}${suffix:+_$suffix}/dpc3/sizes_$(basename ${bin})/           &>> /cluster/work/romankb/pyrunner_latency_fixed_dpc3_$(basename ${bin}).log &
        # CVP-1
        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=${mem_per_cpu} -n1 -c${max_core_count} -t$timelimit -o /cluster/work/romankb/latency-server-${dir}-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-server-${dir}-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --traces_directory /cluster/work/romankb/dataset/CVP1public                       --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}${suffix:+_$suffix}/cvp1/sizes_$(basename ${bin})/           &>> /cluster/work/romankb/pyrunner_latency_fixed_cvp1_$(basename ${bin}).log &
        # LLBP Benchmarks --intel
#        srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=${mem_per_cpu} -n1 -c${max_core_count} -t$timelimit -o /cluster/work/romankb/latency-server-${dir}-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-server-${dir}-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/LLBP                       --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}${suffix:+_$suffix}/LLBP/sizes_$(basename ${bin})/           &>> /cluster/work/romankb/pyrunner_latency_fixed_llbp_$(basename ${bin}).log &
        # Google Traces / No performance benchmark as no dependency information --intel
#         srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=${mem_per_cpu} -n1 -c${max_core_count} -t$timelimit -o /cluster/work/romankb/latency-whiskey-${dir}-$(basename ${bin})-%j.out -e /cluster/work/romankb/latency-whiskey-${dir}-$(basename ${bin})-%j.err python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/google/whiskey             --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}${suffix:+_$suffix}/google_whiskey/sizes_$(basename ${bin})/ &>> /cluster/work/romankb/pyrunner_latency_fixed_google_whiskey_$(basename ${bin}).log &
#         srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=${mem_per_cpu} -n1 -c${max_core_count} -t$timelimit -o /cluster/work/romankb/latency-delta-${dir}-$(basename ${bin})-%j.out   -e /cluster/work/romankb/latency-delta-${dir}-$(basename ${bin})-%j.err   python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/google/delta               --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}${suffix:+_$suffix}/google_delta/sizes_$(basename ${bin})/   &>> /cluster/work/romankb/pyrunner_latency_fixed_google_delta_$(basename ${bin}).log &
#         srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=${mem_per_cpu} -n1 -c${max_core_count} -t$timelimit -o /cluster/work/romankb/latency-charlie-${dir}-$(basename ${bin})-%j.out -e /cluster/work/romankb/latency-charlie-${dir}-$(basename ${bin})-%j.err python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/google/charlie             --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}${suffix:+_$suffix}/google_charlie/sizes_$(basename ${bin})/ &>> /cluster/work/romankb/pyrunner_latency_fixed_google_charlie_$(basename ${bin}).log &
#         srun --account=share-ie-idi -J num-collection-$(basename ${bin}) --mail-user=romankb@ntnu.no --mail-type=FAIL --mem-per-cpu=${mem_per_cpu} -n1 -c${max_core_count} -t$timelimit -o /cluster/work/romankb/latency-merced-${dir}-$(basename ${bin})-%j.out  -e /cluster/work/romankb/latency-merced-${dir}-$(basename ${bin})-%j.err  python ~/ChampSim/data_collector.py --warmup ${warmup} --evaluation ${simulation} --experiment_executable ${bin} --intel --traces_directory /cluster/work/romankb/dataset/google/merced              --nosub --output_dir /cluster/work/romankb/results/${dir}_${count}${suffix:+_$suffix}/google_merced/sizes_$(basename ${bin})/  &>> /cluster/work/romankb/pyrunner_latency_fixed_google_merced_$(basename ${bin}).log &
    done
done

for job in `jobs -p`
do
    echo $job
    wait $job
done
