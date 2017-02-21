#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "freq_list.h"
#include "worker.h"


// void array_sort(FreqRecord* head, FreqRecord** head){
// 	//
// }


int main(int argc, char **argv) {
	char ch; char path[PATHLENGTH]; char word[MAXWORD];
	//store all valid directory in a 2d array 
	char all_paths[MAXFILES][PATHLENGTH];
	char *startdir = ".";
	//counting the total number of directory
	int dir_num = 0;
	while((ch = getopt(argc, argv, "d:")) != -1) {
		switch (ch) {
			case 'd':
			startdir = optarg;
			break;
			default:
			fprintf(stderr, "Usage: queryone [-d DIRECTORY_NAME]\n");
			exit(1);
		}
	}
	// Open the directory provided by the user (or current working directory)
	
	DIR *dirp;
	if((dirp = opendir(startdir)) == NULL) {
		perror("opendir");
		exit(1);
	}
	
	/* For each entry in the directory, eliminate . and .., and check
	* to make sure that the entry is a directory*/
		
	struct dirent *dp;
	while((dp = readdir(dirp)) != NULL) {

		if(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || strcmp(dp->d_name, ".svn") == 0){
			continue;
		}
		strncpy(path, startdir, PATHLENGTH);
		strncat(path, "/", PATHLENGTH - strlen(path) - 1);
		strncat(path, dp->d_name, PATHLENGTH - strlen(path) - 1);

		//create complete path for listfile and indexfile for check if the index and listfile exist in directory or not.
		char* index = "/index";
		char* name = "/filenames";
		char listfile[(strlen(path) + strlen(index) + 1)];
		char namefile[(strlen(path) + strlen(name) + 1)];
		strcpy(listfile, path);
		strcpy(namefile, path);
		strcat(listfile, index);
		strcat(namefile, name);

		struct stat sbuf, lbuf, nbuf;
		if(stat(path, &sbuf) == -1) {
			//This should only fail if we got the path wrong
			// or we don't have permissions on this entry.
			perror("stat");
			exit(1);
		}
		// Only call run_worker if it is a directory and if the directory contains files: index and filenames.
		// Otherwise ignore it.
		if(S_ISDIR(sbuf.st_mode) && (stat(listfile, &lbuf) == 0) && (stat(namefile, &nbuf) == 0)) {
			//copy the path of each subdirectory to the all_paths array
			strncpy(all_paths[dir_num], path, PATHLENGTH);
			//count the number of subdirectories
			dir_num++;
		}
	}

	int write_pipe[dir_num][2];
	int read_pipe[dir_num][2];
	pid_t pid;
	int file_id = 0;
	int counter = 0;

	FreqRecord master_array[MAXRECORDS];
	FreqRecord *fr = malloc(sizeof(FreqRecord));
	//check malloc failure.
	for (file_id = 0; file_id < dir_num; file_id++){
		//based on the total dir num to create that many numbers of pipe
		if ( (pipe(write_pipe[file_id]) == -1 ) || (pipe(read_pipe[file_id]) == -1)){
			perror("Pipe failed.");
			exit(1);
		}
		//forking it
		pid = fork();
		switch(pid){
			//fork fails
			case -1:
				perror("Fork failed");
        		exit(1);

			//child
			case 0:
				// //run worker for each child
				// //reads from read_pipe[1], writes to write_pipe[0]
				if (close(write_pipe[file_id][1]) != 0) {
					perror("close");
					exit(1);
				}

				//close the read_pipe
				if (close(read_pipe[file_id][0]) != 0) {
					perror("close");
					exit(1);
				}

				for (counter = 0; counter < file_id; counter++) {
					//close the write_pipe
					if (close(write_pipe[counter][1]) != 0) {
						perror("close");
						exit(1);
					}
					//close the read_pipe
					if (close(read_pipe[counter][0]) != 0) {
						perror("close");
						exit(1);
					}
				}
				run_worker(all_paths[file_id], write_pipe[file_id][0], read_pipe[file_id][1]);
				exit(0);

			//parent			
			default:
				if (close(write_pipe[file_id][0]) != 0) {
					perror("close");
					exit(1);
				}
					
				//close the write_pipe
				if (close(read_pipe[file_id][1]) != 0) {
					perror("close");
					exit(1);
				}
				break;
		}
	}
	//forking done
	//start from here
	while (1) {
		int i = 0; int child_id = 0; int master_index = 0;
		// FreqRecord* master_array= malloc(MAXRECORDS* sizeof(FreqRecord));
		// check_memory_fail(master_array);
		while(master_index < MAXRECORDS){
			master_array[master_index].freq = 0;
			strcpy(master_array[master_index].filename, "\0");
			master_index++;
		}
		master_index = 0;

		memset(master_array, '\0', MAXRECORDS);
		printf("Enter the word to search:\n");
		int bytes = 0;

		//if user enters ctrl-D, break out of the infinite loop.
		if ((bytes = read(STDIN_FILENO, word, MAXWORD)) == 0) {
			printf("Exiting\n");
			exit(0);
		}
		//otherwise continue
		else {
			
			word[bytes - 1] = '\0';
			// write to worker
			for (i = 0; i < dir_num; i++) {
				if (write(write_pipe[i][1], word, MAXWORD) < 0) {
					perror("write");
					exit(1);
				}
			}
			//read from worker
			while (child_id < dir_num) {
				bytes = read(read_pipe[child_id][0], fr, sizeof(FreqRecord));
				// while 1.reading reasult is not 0
				//       2.the result of fr->freq is not 0
				//       3.total length of the master array must be within the MAXRECORDS.
				//only if the previous conditon satis fy the loop will continue
				while ((bytes > 0) && (fr->freq != 0) && (master_index < MAXRECORDS)) {
					//loop through next child.
					master_array[master_index] = *fr;
					qsort(master_array, master_index, sizeof(FreqRecord), cmp);
					check_memory_fail(master_array);
					master_index++;
					bytes = read(read_pipe[child_id][0], fr, sizeof(FreqRecord));
				}
				child_id++;
			}
			// if read goes wrtong
			if (bytes < 0){
				perror("Read failed.");
				exit(1);
			}
		}
		// finish adding and sorting, print the result
		print_freq_records(master_array);
	}
	for (file_id = 0; file_id < dir_num; file_id++) {
		if (close(write_pipe[file_id][1]) != 0) exit(1);
		if (close(write_pipe[file_id][0]) != 0) exit(1);
		if (close(read_pipe[file_id][0]) != 0) exit(1);
		if (close(read_pipe[file_id][1]) != 0) exit(1);
	}

	if (wait(NULL) == -1) {
		perror("wait");
		exit(1);
	}
	free(fr);
	return 0;
}
