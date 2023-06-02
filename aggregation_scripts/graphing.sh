# IPC/MPKI
export filename=ipc.tsv; export output_name='ipc_avg_bar'; plotgen -i ./${filename} --palette bright --add-column AVG --apply-func add --apply-icolumns :7 7: --print --normalise-to-irow 1 --apply-func sub 1 --apply-icolumns : --x-type category --y-tick-format ',.2%' --print --plot bar --line-mode lines+markers --file ./raw_data/${output_name}.tsv -o ./graphs/${output_name}.html

# Block and similar stats
export filename=num_cl_with_block_size.tsv; export output_name='block_sizes_accum_avg_bar'; plotgen -i */${filename} --join index --no-columns --icolumn-names 1: --trace-names */${filename} --palette bright --add-column AVG --apply-func add --apply-icolumns :7 7: --apply-func cumsum --apply-columns : --print --normalise-function max --normalise-rows --print --plot bar --line-mode lines+markers --file ./raw_data/${output_name}.tsv -o ./graphs/${output_name}.html
