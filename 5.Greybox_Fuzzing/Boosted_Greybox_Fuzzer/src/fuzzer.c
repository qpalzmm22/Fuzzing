/*
 * MADE BY MJ-SEO
 * COPY RIGHT ON MJ-SEO. DO NOT COPY WITHOUT PERMISSION ON I WILL SUE. 
 */

#include "../include/fuzzer.h"
#include "../include/create_input.h"
#include "../include/coverage.h"
#include "../include/sched.h"

//#define DEBUG

#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <assert.h>

static test_config_t fuzz_config;	
static int in_pipes[2] ;	
static int out_pipes[2] ;
static int err_pipes[2] ;
static pid_t child_pid;
// static char input_files[100][4096];
static seed_t seed[8192];	// Capacity of Seed?

static int gcov_flag;

static gcov_src_t* gcov_src;

void
time_handler(int sig){
	if(sig == SIGALRM){
		perror("timeout ");
		kill(child_pid, SIGINT);
	}
}

void
make_tempdir(char* dir_name){
	char template[] = "tmp.XXXXXX";
	char* tempdir = mkdtemp(template);
	if(tempdir == NULL){
		perror("Mkdtemp Error\n");
		exit(1);
	}
	strcpy(dir_name, tempdir);
}

void
fuzzer_init(test_config_t * config, char* dir_name, int* flag){
	if(config->mutation == 1){	// Mutation
		DIR* inp_dir;
		struct dirent* dp = NULL;
		int n_inputs = 0;

		if((inp_dir = opendir(config->mutation_dir)) == NULL){
			perror("[fuzzer_init] Target dir open failed");
			exit(1);
		}

		fuzz_config.mutation_dir = config->mutation_dir;

		while((dp = readdir(inp_dir)) != NULL){
			if(dp->d_type == DT_REG){
//				printf("[FILE] %s\n", dp->d_name);
//				sprintf(input_files[n_inputs], "%s/%s", config->mutation_dir, dp->d_name);
				   
				sprintf(seed[n_inputs].data, "%s/%s", config->mutation_dir, dp->d_name);	
//				printf("[Fuzz init] %s[%d]\n", seed[n_inputs].data, n_inputs);
				seed[n_inputs].length = strlen(dp->d_name); // TODO strlen in file
				seed[n_inputs].num_executed = 1;
				n_inputs++;
			}		
		}

		fuzz_config.mutation = n_inputs;
		closedir(inp_dir);
	}


	fuzz_config.greybox = config->greybox;
	fuzz_config.init_seed = config->init_seed;

	if(config->f_min_len < MINLEN || config->f_max_len > MAXLEN){
		perror("[fuzzer_init] - Fuzzer Length Error\n");
		exit(1);
	}
	if(config->f_min_len > config->f_max_len){
		perror("[fuzzer_init] - Fuzzer Length Error\n");
		exit(1);
	}
	if(config->f_char_start < 0 || config->f_char_start > 127){
		perror("[fuzzer_init] - Fuzzer Config Error\n");
		exit(1);
	}
	if(config->f_char_range < 0 || (config->f_char_start + config->f_char_range) > 255){
		perror("[fuzzer_init] - Fuzzer Range Error\n");
		exit(1);
	}
	if(strlen(config->binary_path) > MAXLEN){
		perror("[fuzzer_init] - Fuzzer Path Error\n");
		exit(1);
	}

	fuzz_config.f_min_len = config->f_min_len;
	fuzz_config.f_max_len = config->f_max_len;
	fuzz_config.f_char_start = config->f_char_start;
	fuzz_config.f_char_range = config->f_char_range;

	strcpy(fuzz_config.binary_path, config->binary_path);

	fuzz_config.trial = config->trial;
	fuzz_config.number_of_source = config->number_of_source;
	fuzz_config.source_path = config->source_path;
	fuzz_config.curr_dir = config->curr_dir;
	fuzz_config.option_num = config->option_num;
	
	fuzz_config.exponent = config->exponent;

	if(config->cmd_args != NULL) {
		if(config->number_of_source > 0){
			fuzz_config.cmd_args = (char **)malloc(sizeof(char *) * (config->option_num + 3)) ; 
			fuzz_config.cmd_args[0] = (char *)malloc(sizeof(char) * (strlen(config->binary_path) + 1)) ;
			strcpy(fuzz_config.cmd_args[0], config->binary_path);

			for(int i = 0 ; i < config->option_num; i++) {
				int length = strlen(config->cmd_args[i]) + 1; 
				fuzz_config.cmd_args[i+1] = (char *)malloc(sizeof(char) * length) ; 
				strcpy(fuzz_config.cmd_args[i+1], config->cmd_args[i]) ;
			}
			fuzz_config.cmd_args[config->option_num + 2] = NULL; 

		}
		else{
			fuzz_config.cmd_args = (char **)malloc(sizeof(char *) * (config->option_num + 2)) ; 
			fuzz_config.cmd_args[0] = (char *)malloc(sizeof(char) * (strlen(config->binary_path) + 1)) ;
			strcpy(fuzz_config.cmd_args[0], config->binary_path);

			for(int i = 0 ; i < config->option_num; i++) {
				int length = strlen(config->cmd_args[i]) + 1; 
				fuzz_config.cmd_args[i+1] = (char *)malloc(sizeof(char) * length) ; 
				strcpy(fuzz_config.cmd_args[i+1], config->cmd_args[i]) ;
			}
			fuzz_config.cmd_args[config->option_num + 1] = NULL; 
		}
	}
	else{
		if(config->number_of_source > 0){
			fuzz_config.cmd_args = (char **)malloc(sizeof(char *) * 3); 
			fuzz_config.cmd_args[0] = (char *)malloc(sizeof(char) * (strlen(config->binary_path) + 1)); 
			strcpy(fuzz_config.cmd_args[0], config->binary_path); 
			fuzz_config.cmd_args[2] = NULL; 

		}
		else{
			fuzz_config.cmd_args = (char **)malloc(sizeof(char *) * 2); 
			fuzz_config.cmd_args[0] = (char *)malloc(sizeof(char) * (strlen(config->binary_path) + 1)); 
			strcpy(fuzz_config.cmd_args[0], config->binary_path); 
			fuzz_config.cmd_args[1] = NULL; 
		}
	}

	fuzz_config.need_args = config->need_args;

	fuzz_config.timeout = config->timeout;

	fuzz_config.oracle = config->oracle;

	if(config->number_of_source > 0){
		for(int i=0; i<config->number_of_source; i++){
			char* s_path = (char*)malloc(sizeof(char) * PATH_MAX);
			sprintf(s_path, "%s%s", config->source_path, config->sources[i]);
			assert(access(s_path, F_OK) == 0 && "Target Source Not Exiset!");	
			free(s_path);
		}
		gcov_flag = 1;
		fuzz_config.sources = config->sources;
	}

	make_tempdir(dir_name);
}

void
execute_prog(test_config_t * config, char* input, int input_size, char* dir_name, int file_num){
	char* input_name = (char*)malloc(sizeof(char)*32);
	sprintf(input_name, "%s/input%d", dir_name, file_num);
	FILE* input_file = fopen(input_name, "wb");
	if(input_file == NULL){
		perror("execute_prog: Input-file error");
		exit(1);
	}

	fwrite(input, 1, input_size, input_file);

	fclose(input_file);

	dup2(in_pipes[0], 0);
	close(in_pipes[0]);
	close(in_pipes[1]);

	dup2(out_pipes[1], 1);	
	dup2(err_pipes[1], 2);
	
	close(out_pipes[0]);
	close(err_pipes[0]);


	if(config->need_args == 1){
		execv(config->binary_path, config->cmd_args);
	}
	else{
		execl(config->binary_path, config->binary_path, NULL);
	}

	perror("[excute_prog] - Execution Error\n");
	exit(1);
}

int
get_info(test_config_t * config, char* input, int input_size, char* dir_name, int file_num){
	close(out_pipes[1]);
	close(err_pipes[1]);

	write(in_pipes[1], input, input_size);
	close(in_pipes[1]);

	int exit_code;
	wait(&exit_code);

	char buffer[PATH_MAX];
	ssize_t s;

#ifdef DEBUG
	printf("[Get_info] input(%d): %s\n", input_size, input);
#endif

	char* output_name = (char*)malloc(sizeof(char)*16);
	sprintf(output_name, "%s/output%d", dir_name, file_num);
	FILE* output_file = fopen(output_name, "wb");

	if(output_file == NULL){
		perror("get_info: Output-file error");
		exit(1);
	}

	while((s = read(out_pipes[0], buffer, PATH_MAX))> 0){
		if(fwrite(buffer, 1, s, output_file) != s){
			perror("Output File Make Error");
		}
	}

	char* err_name = (char*)malloc(sizeof(char)*16);
	sprintf(err_name, "%s/error%d", dir_name, file_num);
	FILE* err_file = fopen(err_name, "wb");

	if(err_file == NULL){
		perror("get_info: Error-file error");
		exit(1);
	}

	while((s = read(err_pipes[0], buffer, PATH_MAX)) > 0){
		if(fwrite(buffer, 1, s, err_file) != s){
			perror("Error File Make Error");
		}
	}

	close(in_pipes[0]);
	close(out_pipes[0]);
	close(err_pipes[0]);

	free(output_name);
	free(err_name);
	fclose(output_file);
	fclose(err_file);

	return exit_code;
}


void
run_gcov(char* source, int idx){
	pid_t gcov_child = fork();
	
	if(gcov_child == -1){
		perror("Fork Error");
		exit(1);
	}

	printf("[DEBUG] Gcov_run with source %s/%d\n", source, idx);

	char* s_path = (char*)malloc(sizeof(char) * PATH_MAX);
	sprintf(s_path, "%s%s", fuzz_config.source_path, fuzz_config.sources[idx]);

#ifdef DEUBG
	printf("[DEBUG] run_gcov: sourcessss %s\n", s_path);
#endif
	if(gcov_child == 0){
		if(fuzz_config.curr_dir == 1){
			close(1);
			close(2);
			execl("/usr/bin/gcov", "gcov", "-b", "-c", source, NULL);
			perror("run_gcov: Execution Error!");
		}
		else{
			close(1);
			close(2);
			execl("/usr/bin/gcov", "gcov", "-b", "-c", s_path, NULL);
			perror("run_gcov: Execution Error!");
		}		
		return;
	}
	else if(gcov_child > 0){
		int exit_code;
		wait(&exit_code);
		free(s_path);
		if(exit_code != 0){
			perror("GCOV Execute Error!");
			return;
		}
	}
	else{
		perror("run_gcov: Fork Error!");
		exit(1);
	}
}

int
run(test_config_t* config, char* input, int input_size, char* dir_name, int file_num){
	if ( pipe(in_pipes) == -1 || pipe(out_pipes) == -1 || pipe(err_pipes) == -1 ) {
		perror("Pipe Error\n") ;
		exit(1) ;
	}
	
	int return_code;

	alarm(config->timeout);

	child_pid = fork();
	if(child_pid == -1){
		perror("Fork Error");
		exit(1);
	}

	if (child_pid == 0) {
		execute_prog(config, input, input_size, dir_name, file_num) ;
	}
	else if (child_pid > 0) {
		return_code = get_info(config, input, input_size, dir_name, file_num) ;
	}
	else {
		perror("Fork Error\n") ;
		exit(1) ;
	}

	return return_code;
}

void gcov_init(){
	
	for( int i = 0 ; i < fuzz_config.number_of_source; i++){
		printf("[DEBUG] Before line: %d\n", gcov_src[i].tot_branches);
		
		gcov_src[i].gcov_line = get_gcov_line(fuzz_config.sources[i], &gcov_src[i].tot_lines, &gcov_src[i].tot_branches);

		printf("[DEBUG] fuzzer_main, lines:%d\n", gcov_src[i].gcov_line);
		printf("[DEBUG] After line:%d\n", gcov_src[i].tot_branches);

		gcov_src[i].bitmap = (char*)malloc(sizeof(char) * gcov_src[i].gcov_line);
		gcov_src[i].branch_bitmap = (char*)malloc(sizeof(char) * gcov_src[i].gcov_line);

		memset(gcov_src[i].bitmap, 0, sizeof(char) * gcov_src[i].gcov_line);
		memset(gcov_src[i].branch_bitmap, 0, sizeof(char) * gcov_src[i].gcov_line);
	//	gcov_src[i].hash_size = 0;
	}
}

void
show_result(int* return_code, int* prog_results, int trial, double exe_time){
	int return_checker = 0;
	for(int i=0; i<trial; i++){
//		printf("[%d] Return code: %d, Result: %d\n", i, return_code[i], prog_results[i]);
	}
	printf("It took the fuzzer %lf seconds to generate and execute %d inputs.\n", exe_time, trial);
}

void 
show_gcov(int* return_code, gcov_t** gcov_results, int trial, int n_src){
	int pass = 0;
	int fail = 0;
	for(int i=0; i<trial; i++){
		if(return_code[i] == 0) pass++;
		else fail++;
	}
	
	printf("===========================================Fuzzer Log============================================\n");
	for(int i=0; i<trial; i++){
		 printf("    \t\t\t\t\t---[Input %d]---\n", i);
		for(int j=0; j<n_src; j++){
			printf("[Source: %s] ", fuzz_config.sources[j]); 
			printf("Line: %d/%d ", gcov_results[i][j].line, gcov_src[j].tot_lines);
			printf("Union: %d ", gcov_results[i][j].union_line);
			printf("Coverage: %lf    ", (double)gcov_results[i][j].union_line/gcov_src[j].tot_lines);

			printf("Branch: %d/%d ", gcov_results[i][j].branch_line, gcov_src[j].tot_branches);
			printf("Union: %d ", gcov_results[i][j].branch_union_line);
			printf("Coverage: %lf   \n\n", (double)gcov_results[i][j].branch_union_line/gcov_src[j].tot_branches);
		}
	}
	printf("=====================================================================================================\n");
	
	printf("\n===========================Fuzzer Summary===========================\n");
	printf("* Trial : %d\n", trial);
	printf("* Pass  : %d\n", pass);
	printf("* Fail  : %d\n", fail);
	printf("* Line Coverage : (%d/%d) %lf\n", gcov_results[trial-1][0].union_line, gcov_src[0].tot_lines, (double)gcov_results[trial-1][0].union_line / gcov_src[0].tot_lines);
	printf("* Branch Coverage : (%d/%d) %lf\n", gcov_results[trial-1][0].branch_union_line, gcov_src[0].tot_branches, (double)gcov_results[trial-1][0].branch_union_line / gcov_src[0].tot_branches );

//	for(int i=0; i<gcov_src[0].hash_size; i++){
	//	printf("Hash: %d\n", gcov_src[0].hash_table[i]);
//	}
//	printf("[DEBUG] num_of_hash: %d\n", gcov_src[0].hash_size);
	int flag = 0;
/*	for(int i=0; i<gcov_src[0].hash_size; i++){
		for(int j=i+1; j<gcov_src[0].hash_size; j++){
			if(gcov_src[0].hash_table[i] == gcov_src[0].hash_table[j]){
				printf("[!!!!!!!!!!!!!11] dup!! in %d\n", j);
			}
		}
		flag++;
	}
	printf("[DEBUG] flag: %d\n", flag);
*/
	printf("=====================================================================\n");
	// TODO multiple source
}

void 
make_csv(gcov_t** gcov_results, int trial, int n_src){
	FILE* csv;
	csv = fopen("result.csv", "ab");
	if(csv == NULL){
		perror("make_csv: File open error");
		exit(1);
	}
	
	for(int i=0; i<trial; i++){
		char* buffer = (char*)malloc(sizeof(char) * 30);
		memset(buffer, 0, 30);
/*		if(i==1){
			fputs("Trial,",csv);
			fputs("Branch\n",csv);
		}
		printf("[DEBUG] trial: %d  ", i);	
		printf("Line: %d  ", gcov_results[i-1][0].union_line);
		printf("Branch: %d\n", gcov_results[i-1][0].branch_union_line);
*/
		sprintf(buffer, "%d\n", gcov_results[i][0].branch_union_line);
		fwrite(buffer, 1, strlen(buffer), csv);
		free(buffer);
	}
		
	fputs("\n", csv);
	fclose(csv);
}

void
gcov_result_free(gcov_t** result, int trial){
	for(int i=0; i<trial; i++){
		free(result[i]);
	}
	free(result);
}

void
gcov_src_free(gcov_src_t* gcov_src){
//gcov_src_free(gcov_src_t* gcov_src){
	for(int i=0; i<fuzz_config.number_of_source; i++){
		free(gcov_src[i].bitmap);
		free(gcov_src[i].branch_bitmap);
	}
	free(gcov_src);
}

void
fuzz_free(test_config_t fuzz_config){
	if(fuzz_config.cmd_args != NULL) {
		if(fuzz_config.number_of_source > 0){
			for(int i=0; i<fuzz_config.option_num+3; i++){
				free(fuzz_config.cmd_args[i]);
			}
		}
		else{
			for(int i=0; i<fuzz_config.option_num+2; i++){
				free(fuzz_config.cmd_args[i]);
			}
		}
	}
	else{
		if(fuzz_config.number_of_source > 0){
			for(int i=0; i<3; i++){
				free(fuzz_config.cmd_args[i]);
			}

		}
		else{
			for(int i=0; i<2; i++){
				free(fuzz_config.cmd_args[i]);
			}
		}
	}
	
	free(fuzz_config.cmd_args);
}

void
fuzzer_main(test_config_t* config){
	srand((unsigned int)time(NULL));

	signal(SIGALRM, time_handler);

	char dir_name[32];

	fuzzer_init(config, dir_name, &gcov_flag);	

	int* prog_results = (int*)malloc(sizeof(int) * (fuzz_config.trial + 1));
	int* return_code = (int*)malloc(sizeof(int) * (fuzz_config.trial + 1));
	gcov_t** gcov_results= (gcov_t**)malloc(sizeof(gcov_t*) * fuzz_config.trial); // trial + 1
	for(int trial_n=0; trial_n<fuzz_config.trial; trial_n++){
		gcov_results[trial_n] = (gcov_t*)malloc(sizeof(gcov_t) * fuzz_config.number_of_source);
	}

	gcov_src = (gcov_src_t*)malloc(sizeof(gcov_src_t) * (fuzz_config.number_of_source));
	
	// gcov malloc and memset

	clock_t t_start = clock();
	for(int i = 0; i < fuzz_config.trial; i++){

		char* input = (char*)malloc(sizeof(char)* FUZZ_MAX); 
		memset(input, 0, FUZZ_MAX);

		int fuzz_len;
		int choosed;

		if(fuzz_config.mutation > 0){
			if(i<fuzz_config.init_seed){
				printf("[DEBUG] seed: %s\n", seed[i%(fuzz_config.mutation)].data);
				fuzz_len = mutational_input(input, seed[i%(fuzz_config.mutation)].data, 0);
			}
			else{
				fuzz_len = mutational_input(input, choose_seed(seed, fuzz_config.mutation, fuzz_config.exponent, &choosed), 1);
			}
		}
		else{
			fuzz_len = create_input(&fuzz_config, input); // Generage Random Input
		}
	

#ifdef DEBUG
		printf("[Trial %d]Input: %s(%d)\n", i, input, fuzz_len);
#endif
		fuzz_config.cmd_args[fuzz_config.option_num + 1] = input; 

#ifdef DEBUG
		printf("[execute_prog] cmd_args[%d]: %s\n", i, fuzz_config.cmd_args[i]);
#endif
		return_code[i] = run(&fuzz_config, input, fuzz_len, dir_name, i);
		
#ifdef DEBUG
		printf("fuzz_config.number_of_source %d \n",fuzz_config.number_of_source);
#endif

		for(int n_src=0; n_src<fuzz_config.number_of_source; n_src++){ 

			run_gcov(fuzz_config.sources[n_src], n_src);
			if(i == 0){
				gcov_init();
			}

			int new_mutate;
			new_mutate =  read_gcov_coverage(fuzz_config.sources[n_src], gcov_results, i, n_src, &gcov_src[n_src]);
			
			if(new_mutate == 1 && fuzz_config.mutation>0 && fuzz_config.greybox == 1 && i >= fuzz_config.init_seed){	// TODO condition.....
				fuzz_config.mutation++;
				sprintf(seed[fuzz_config.mutation-1].data, "%s/input%d", config->mutation_dir, fuzz_config.mutation); 

				seed[fuzz_config.mutation-1].num_executed = 1;
//					printf("[DEBUG] choosed: %d\n", choosed);
				if(seed[choosed].num_executed > 1){
					seed[choosed].num_executed -= 1;
				}

				char* input_name = (char*)malloc(sizeof(char)*25);
				sprintf(input_name, "%s/input%d", fuzz_config.mutation_dir, fuzz_config.mutation);
				FILE* new_inp_file = fopen(input_name, "wb");
//					printf("[DEBUG] new_inp_file: %s\n", input_name);
				if(new_inp_file == NULL){
					perror("new_mutate: FILE Open Failed");
				}
#ifdef DEBUG
	printf("[DEBUG] new_inp: %sn_srcn", input);	
#endif		
				fwrite(input, 1, fuzz_len, new_inp_file);
				free(input_name);
				fclose(new_inp_file);
			}
		}
		

		for(int i=0; i<fuzz_config.number_of_source; i++){
			if(fuzz_config.curr_dir == 1){	// gcov in current directory
				gcda_remove(fuzz_config.sources[i], 0x0);
			} else{
				gcda_remove(fuzz_config.sources[i], fuzz_config.source_path);
			}
		}
		
		free(input);
		fuzz_config.oracle(dir_name, i, prog_results, return_code[i]);
	}
	clock_t t_end = clock();

	double d_time = (double)(t_end - t_start)/CLOCKS_PER_SEC;

	if(gcov_flag == 1){
		show_gcov(return_code, gcov_results, fuzz_config.trial, fuzz_config.number_of_source);
		show_result(return_code, prog_results, fuzz_config.trial, d_time);
	}
	else{
		show_result(return_code, prog_results, fuzz_config.trial, d_time);
	}

	make_csv(gcov_results, fuzz_config.trial, fuzz_config.number_of_source);

	free(prog_results);
	free(return_code);
	
	gcov_result_free(gcov_results, fuzz_config.trial);
	gcov_src_free(gcov_src);
	
//	fuzz_free(fuzz_config);
}
