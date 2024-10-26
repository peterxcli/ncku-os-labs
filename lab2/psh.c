#include <stdlib.h>
#include <stdio.h>
#include "include/shell.h"
#include "include/command.h"

int history_count;
char *history[MAX_RECORD_NUM];

int main(int argc, char *argv[])
{
	printf("psh: Peter's Shell\n");
	history_count = 0;
	for (int i = 0; i < MAX_RECORD_NUM; ++i)
    	history[i] = (char *)malloc(BUF_SIZE * sizeof(char));

	shell();

	for (int i = 0; i < MAX_RECORD_NUM; ++i)
    	free(history[i]);

	return 0;
}
