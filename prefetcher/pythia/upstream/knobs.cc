#include "compat.hpp"
namespace pythia_impl::knob {
/* Scooby */
	float    scooby_alpha = 0.006508802942367162;
	float    scooby_gamma = 0.556300959940946;
	float    scooby_epsilon = 0.0018228444309622588;
	uint32_t scooby_state_num_bits = 10;
	uint32_t scooby_max_states = 1024;
	uint32_t scooby_seed = 200;
	string   scooby_policy = "EGreedy";
	string   scooby_learning_type = "SARSA";
	vector<int32_t> scooby_actions = {1,3,4,5,10,11,12,22,23,30,32,-1,-3,-6,0};
	uint32_t scooby_max_actions = 15;
	uint32_t scooby_pt_size = 256;
	uint32_t scooby_st_size = 64;
	int32_t  scooby_reward_none = -4;
	int32_t  scooby_reward_incorrect = -8;
	int32_t  scooby_reward_correct_untimely = 12;
	int32_t  scooby_reward_correct_timely = 20;
	uint32_t scooby_max_pcs = 5;
	uint32_t scooby_max_offsets = 5;
	uint32_t scooby_max_deltas = 5;
	bool     scooby_brain_zero_init = false;
	bool     scooby_enable_reward_all = false;
	bool     scooby_enable_track_multiple = false;
	bool     scooby_enable_reward_out_of_bounds = true;
	int32_t  scooby_reward_out_of_bounds = -12;
	uint32_t scooby_state_type = 1;
	bool     scooby_access_debug = false;
	bool     scooby_print_access_debug = false;
	uint64_t scooby_print_access_debug_pc = 0xdeadbeef;
	uint32_t scooby_print_access_debug_pc_count = 0;
	bool     scooby_print_trace = false;
	bool     scooby_enable_state_action_stats = false;
	bool     scooby_enable_reward_tracker_hit = false;
	int32_t  scooby_reward_tracker_hit = -2;
	bool     scooby_enable_shaggy = false;
	uint32_t scooby_state_hash_type = 11;
	bool     scooby_prefetch_with_shaggy = false;
	bool     scooby_enable_featurewise_engine = true;
	uint32_t scooby_pref_degree = 1; /* default is set to 1 */
	bool     scooby_enable_dyn_degree = true;
	vector<float> scooby_max_to_avg_q_thresholds = {0.5,1,2}; /* depricated */
	vector<int32_t> scooby_dyn_degrees = {1,2,4,4};
	uint64_t scooby_early_exploration_window = 0;
	uint32_t scooby_multi_deg_select_type = 2; /* type 1 is already depricated */
	vector<int32_t> scooby_last_pref_offset_conf_thresholds = {1,3,8};
	vector<int32_t> scooby_dyn_degrees_type2 = {1,2,4,6};
	uint32_t scooby_action_tracker_size = 2;
	uint32_t scooby_high_bw_thresh = 4;
	bool     scooby_enable_hbw_reward = true;
	int32_t  scooby_reward_hbw_correct_timely = 20;
	int32_t  scooby_reward_hbw_correct_untimely = 12;
	int32_t  scooby_reward_hbw_incorrect = -14;
	int32_t  scooby_reward_hbw_none = -2;
	int32_t  scooby_reward_hbw_out_of_bounds = -12;
	int32_t  scooby_reward_hbw_tracker_hit = -2;
	vector<int32_t> scooby_last_pref_offset_conf_thresholds_hbw = {1,3,8};
	vector<int32_t> scooby_dyn_degrees_type2_hbw = {1,2,4,6};

	/* Learning Engine */
	bool     le_enable_trace{};
	uint32_t le_trace_interval{};
	string   le_trace_file_name{};
	uint32_t le_trace_state{};
	bool     le_enable_score_plot{};
	vector<int32_t> le_plot_actions{};
	string   le_plot_file_name{};
	bool     le_enable_action_trace{};
	uint32_t le_action_trace_interval{};
	std::string le_action_trace_name{};
	bool     le_enable_action_plot{};

	/* Featurewise Learning Engine */
	vector<int32_t> le_featurewise_active_features = {0,10};
	vector<int32_t> le_featurewise_num_tilings = {3,3};
	vector<int32_t> le_featurewise_num_tiles = {128,128};
	vector<int32_t> le_featurewise_hash_types = {2,2};
	vector<int32_t> le_featurewise_enable_tiling_offset = {1,1};
	float le_featurewise_max_q_thresh = 0.50;
	bool le_featurewise_enable_action_fallback = true;
	vector<float> le_featurewise_feature_weights = {1.00,1.00};
	bool le_featurewise_enable_dynamic_weight = false;
	float le_featurewise_weight_gradient = 0.001;
	bool le_featurewise_disable_adjust_weight_all_features_align = true;
	bool le_featurewise_selective_update = false;
	uint32_t le_featurewise_pooling_type = 2; /* max pooling */
	bool le_featurewise_enable_dyn_action_fallback = true;
	uint32_t le_featurewise_bw_acc_check_level = 1;
	uint32_t le_featurewise_acc_thresh = 2;

	bool 			le_featurewise_enable_trace = false;
	uint32_t		le_featurewise_trace_feature_type{};
	string 			le_featurewise_trace_feature{};
	uint32_t 		le_featurewise_trace_interval{};
	uint32_t 		le_featurewise_trace_record_count{};
	std::string 	le_featurewise_trace_file_name{};
	bool 			le_featurewise_enable_score_plot{};
	vector<int32_t> le_featurewise_plot_actions{};
	std::string 	le_featurewise_plot_file_name{};
	bool 			le_featurewise_remove_plot_script{};
}
