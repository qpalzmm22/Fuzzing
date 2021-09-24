#include "../include/fuzzer.h"
#include <dirent.h>
#include <sys/types.h>

void
config_setting(test_config_t* config){
	config->mutation_dir = "./input/bc_inp";
	config->mutation = 1;

	config->trial = 10;
	
	int n_src = 3;
	config->number_of_source = n_src;

	char** src_arr = (char**)malloc(sizeof(char*) * n_src);
	for(int i =0; i<n_src; i++){
		src_arr[i] = (char*)malloc(sizeof(char) * 1024);
	}

	src_arr[0] = "bc.c";
	src_arr[1] = "main.c";
	src_arr[2] = "load.c";

	config->sources = src_arr;
	config->source_path = "../lib/bc/bc/";
	config->curr_dir = 0;

	strcpy(config->binary_path, "../lib/bc/bc/bc");
}

int main(){
	test_config_t config;

	config_init(&config);

	config_setting(&config);

	fuzzer_main(&config);
}
