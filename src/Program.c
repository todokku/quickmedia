#include "../include/Program.h"
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#define READ_END 0
#define WRITE_END 1

int exec_program(char **args, ProgramOutputCallback output_callback, void *userdata) {
    /* 1 arguments */
    if(args[0] == NULL)
        return -1;
    
    int fd[2];
    if(pipe(fd) == -1) {
        perror("Failed to open pipe");
        return -2;
    }

    pid_t pid = fork();
    if(pid == -1) {
        perror("Failed to fork");
        return -3;
    } else if(pid == 0) { /* child */
        dup2(fd[WRITE_END], STDOUT_FILENO);
        close(fd[READ_END]);
        close(fd[WRITE_END]);

        execv(args[0], args);
        return 0;
    } else { /* parent */
        close(fd[WRITE_END]);

        int status;
        waitpid(pid, &status, 0);
        if(!WIFEXITED(status))
            return -4;

        int exit_status = WEXITSTATUS(status);
        if(exit_status != 0) {
            fprintf(stderr, "Failed to execute program, exit status %d\n", exit_status);
            return -exit_status;
        }

        char buffer[2048];
        int result = 0;

        for(;;) {
            ssize_t bytes_read = read(fd[READ_END], buffer, sizeof(buffer));
            if(bytes_read == 0) {
                break;
            } else if(bytes_read == -1) {
                int err = errno;
                fprintf(stderr, "Failed to read from pipe to program %s, error: %s\n", args[0], strerror(err));
                result = -err;
                break;
            }

            if(output_callback(buffer, bytes_read, userdata) != 0)
                break;
        }

        close(fd[READ_END]);
        return result;
    }
}