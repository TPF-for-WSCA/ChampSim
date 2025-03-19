# IPC/MPKI
export filename=ipc.tsv; export output_name='ipc_avg_bar'; plotgen -i ./${filename} --palette bright --add-column AVG --apply-func add --apply-icolumns :7 7: --print --normalise-to-irow 1 --apply-func sub 1 --apply-icolumns : --x-type category --y-tick-format ',.2%' --print --plot bar --line-mode lines+markers --file ./raw_data/${output_name}.tsv -o ./graphs/${output_name}.html --width 1800 --height 400 -o ./graphs/${output_name}.pdf

# Block and similar stats
export filename=num_cl_with_block_size.tsv; export output_name='block_sizes_accum_avg_bar'; plotgen -i */${filename} --join index --no-columns --icolumn-names 1: --trace-names */${filename} --palette bright --add-column AVG --apply-func add --apply-icolumns :7 7: --apply-func cumsum --apply-columns : --print --normalise-function max --normalise-rows --print --plot bar --line-mode lines+markers --file ./raw_data/${output_name}.tsv -o ./graphs/${output_name}.html --width 1800 --height 400 -o ./graphs/${output_name}.pdf

# Some sort of naming but naming is somehow wrong when executed from bash
 export filename='cpu0_L1I_buffer_num_cl_with_block_size.tsv'; export
 output_name='block_sizes_buffer_accum_avg_bar'; plotgen -i
 ./**/sizes_champsim64k/**/cpu0_L1I_num_cl_with_block_size.tsv -i
 ./**/sizes_champsim32k/**/cpu0_L1I_num_cl_with_block_size.tsv -i
 ./**/sizes_champsim_vcl_buffer_*/**/cpu0_L1I_buffer_num_cl_with_block_size.tsv --no-columns --join
 index --icolumn-names 0: ./**/sizes_champsim64k/**/cpu0_L1I_num_cl_with_block_size.tsv
 ./**/sizes_champsim32k/**/cpu0_L1I_num_cl_with_block_size.tsv $(realpath
 ./**/sizes_champsim_vcl_buffer_*
 /**/cpu0_L1I_buffer_num_cl_with_block_size.tsv | cut -d"/" -f9-11 -) --print




~/plotgen/plotgen -i ./**/sizes_champsim32k/**/cpu0_L1I_num_cl_with_block_size.tsv --no-columns --join index --column-names --filename --apply-func cumsum --apply-columns : --normalise-function max --normalise-icolumns : --renameregex '\./.*/.*/([a-zA-Z\-_0-9\.]+)\.*' --add-function mean --add-row AVG --print --file ./raw_data/accumulated_all_applications.tsv -o ./graphs/accumulated_all_applications.html --width 900 --height 200 -o ./graphs/accumulated_all_applications.pdf 
