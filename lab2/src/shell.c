#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "../include/command.h"
#include "../include/builtin.h"

// ======================= requirement 2.3 =======================
/**
 * @brief 
 * Redirect command's stdin and stdout to the specified file descriptor
 * If you want to implement ( < , > ), use "in_file" and "out_file" included the cmd_node structure
 * If you want to implement ( | ), use "in" and "out" included the cmd_node structure.
 *
 * @param cmd cmd_node structure
 * 
 */
int redirection(struct cmd_node *cmd){
	if (cmd->in_file != NULL) {
		int in = open(cmd->in_file, O_RDONLY);
		if (in == -1) {
			return -1;
		}
		dup2(in, 0);
	}
	if (cmd->out_file != NULL) {
		int out = open(cmd->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (out == -1) {
			return -1;
		}
		dup2(out, 1);
	}
	if (cmd->in != 0) {
		dup2(cmd->in, 0);
	}
	if (cmd->out != 1) {
		dup2(cmd->out, 1);
	}
	return 0;
}
// ===============================================================

// ======================= requirement 2.2 =======================
/**
 * @brief 
 * Execute external command
 * The external command is mainly divided into the following two steps:
 * 1. Call "fork()" to create child process
 * 2. Call "execvp()" to execute the corresponding executable file
 * @param p cmd_node structure
 * @return int 
 * Return execution status
 */
int spawn_proc(struct cmd_node *p)
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid == -1) {
        return -1;
    } else if (pid == 0) {
        // Child process
		int err = redirection(p);
		if (err == -1) {
			return -1;
		}
        execvp(p->args[0], p->args);
        // If execvp returns, it must have failed
        return -1;
    } else {
        // Parent process
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));
    }

	if (WIFSTOPPED(status) || WIFSIGNALED(status)) {
		perror(p->args[0]);
	}

    return WIFEXITED(status);
}
// ===============================================================


// ======================= requirement 2.4 =======================
/**
 * @brief 
 * Use "pipe()" to create a communication bridge between processes
 * Call "spawn_proc()" in order according to the number of cmd_node
 * @param cmd Command structure  
 * @return int
 * Return execution status 
 */
int fork_cmd_node(struct cmd *cmd)
{
	int pipefd[2];
	int status = 0;
	int in = 0;
	struct cmd_node *temp = cmd->head;
	while (temp != NULL) {
		temp->in = in;
		if (temp->next != NULL) {
			pipe(pipefd);
			temp->out = pipefd[1];
		} else {
			temp->out = 1;
		}
		status = spawn_proc(temp);
		if (status != 1) {
			break;
		}
		close(pipefd[1]);
		in = pipefd[0];
		temp = temp->next;
	}

	return status;
}
// ===============================================================


void shell()
{
	while (1) {
		printf(">>> $ ");
		char *buffer = read_line();
		if (buffer == NULL)
			continue;

		struct cmd *cmd = split_line(buffer);
		
		int status = -1;
		// only a single command
		struct cmd_node *temp = cmd->head;
		
		if(temp->next == NULL){
			status = searchBuiltInCommand(temp);
			if (status != -1){
				int in = dup(STDIN_FILENO), out = dup(STDOUT_FILENO);
				if( (in == -1) | (out == -1) )
					perror("dup");
				redirection(temp);
				status = execBuiltInCommand(status,temp);

				// recover shell stdin and stdout
				if (temp->in_file)  dup2(in, 0);
				if (temp->out_file){
					dup2(out, 1);
				}
				close(in);
				close(out);
			}
			else{
				//external command
				status = spawn_proc(cmd->head);
			}
		}
		// There are multiple commands ( | )
		else{
			status = fork_cmd_node(cmd);
		}
		// free space
		while (cmd->head) {
			
			struct cmd_node *temp = cmd->head;
      		cmd->head = cmd->head->next;
			free(temp->args);
   	    	free(temp);
   		}
		free(cmd);
		free(buffer);
		
		if (status == 0)
			break;
	}
}
