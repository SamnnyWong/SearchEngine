#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "worker.h"
#include "freq_list.h"

/* The function get_word should be added to this file */
int cmp (const void * x, const void * y) {
    x = (FreqRecord *)x;
    y = (FreqRecord *)y;
    int x1 = ((FreqRecord *)x)->freq;
    int x2 = ((FreqRecord *)y)->freq;

    if (x1 > x2) {
        return -1;
    } 
    else if (x1 == x2) {
        return 0;
    } 
    
    else { 
        return 1; 
    }
}
void check_memory_fail(FreqRecord* frp){
	if (frp == NULL){
		perror("Error: Memory allocate unsuccessful.");
		exit(EXIT_FAILURE); }}


FreqRecord* get_word(const char *word, Node *head, char **filenames) {
	FreqRecord *res;
	Node *dup_head = head;
	res = malloc((MAXFILES + 1) * sizeof(FreqRecord));
	check_memory_fail(res);
	while ((dup_head != NULL) && (strcmp(dup_head->word, word) != 0)){
		dup_head = dup_head->next;}
	if (dup_head == NULL) { // no match
		res[0].freq = 0;
		//shrink the memory of the array.
		res = realloc(res, sizeof(FreqRecord));
		check_memory_fail(res);}
	else { // find a match
		//counter index memory
		int i = 0; int j = 0;
		for (i = 0, j = 0; i < MAXFILES; i++) {
		// when the next FreqRecord has a freq is not 0, keep looping it.
			if (dup_head->freq[i] != 0){
				res[j].freq = dup_head->freq[i];
				strncpy(res[j].filename, filenames[i], PATHLENGTH);
				j++;
			}
		}
		res[j].freq = 0;
		//shrink the memory of the array.
		res = realloc(res, sizeof(FreqRecord) * (j+1));
		check_memory_fail(res);
	}
	return res;
}


/* Print to standard output the frequency records for a word.
* Used for testing.
*/
void print_freq_records(FreqRecord *frp) {
	int i = 0;
	while(frp != NULL && frp[i].freq != 0) {
		printf("%d    %s\n", frp[i].freq, frp[i].filename);
		i++;}}

/* run_worker
* - load the index found in dirname
* - read a word from the file descriptor "in"
* - find the word in the index list
* - write the frequency records to the file descriptor "out"
*/
void run_worker(char *dirname, int in, int out) {

	Node *head = NULL;
	char listfile[strlen(dirname)+7];
	char namefile[strlen(dirname)+11];
	char word[MAXWORD + 1];
	// initialize the word buffer
	memset(word, '\0', MAXWORD+1);
	char **filenames = init_filenames();
	//create 2 array to stores full path of the index and filenames file.
	strcpy(listfile, dirname);
	strncat(listfile, "/index\0", strlen(dirname)+7);
	strcpy(namefile, dirname);
	strncat(namefile, "/filenames\0", strlen(dirname)+11);
	// check if the given dirname excceed the maximum allowrance or not.
	if (strlen(namefile) > PATHLENGTH){
		perror("directory too long.");
		exit(1);
	}
	//init the linked list with the valid path
	read_list(listfile, namefile, &head, filenames);
	int i, bytes;
	//printf("Now finding word in: %s\n", dirname);
	// everytime this loop starts, it will refresh the bytes of the reading results.
	while ((bytes = read(in, word, MAXWORD)) > 0){
		word[bytes-1] = '\0';
		FreqRecord *fr = get_word(word, head, filenames);
		// when the next FreqRecord has a freq is not 0, keep looping it.
		for (i = 0; fr[i].freq != 0 ; i++){
			if (write(out, &fr[i].freq, sizeof(FreqRecord)) < 0) {
				perror("write");
				exit(1);
			}
		}
		// writes the sennial FreqRecord at the very end
		if (write(out, &fr[i].freq, sizeof(FreqRecord)) < 0) {
			perror("write");
			exit(1);
		}
		//reset the word buffer
		memset(word, '\0', MAXWORD+1);
	}
	// exit when the reading result less than 0
	if (bytes < 0) {
		perror("Read Failed.");
		exit(1);
	}
	//close(in); close(out);
}

