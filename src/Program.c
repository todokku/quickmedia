#include "../include/Program.h"
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define READ_END 0
#define WRITE_END 1

int exec_program(const char **args, ProgramOutputCallback output_callback, void *userdata) {
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

        execvp(args[0], args);
        return 0;
    } else { /* parent */
        close(fd[WRITE_END]);

        int result = 0;
        int status;

        char buffer[2048];

        for(;;) {
            ssize_t bytes_read = read(fd[READ_END], buffer, sizeof(buffer) - 1);
            if(bytes_read == 0) {
                break;
            } else if(bytes_read == -1) {
                int err = errno;
                fprintf(stderr, "Failed to read from pipe to program %s, error: %s\n", args[0], strerror(err));
                result = -err;
                goto cleanup;
            }

            buffer[bytes_read] = '\0';
            if(output_callback && output_callback(buffer, bytes_read, userdata) != 0)
                break;
        }

        if(waitpid(pid, &status, WUNTRACED) == -1) {
            perror("waitpid failed");
            result = -5;
            goto cleanup;
        }

        if(!WIFEXITED(status)) {
            result = -4;
            goto cleanup;
        }

        int exit_status = WEXITSTATUS(status);
        if(exit_status != 0) {
            fprintf(stderr, "Failed to execute program (");
            const char **arg = args;
            while(*arg) {
                if(arg != args)
                    fputc(' ', stderr);
                fprintf(stderr, "'%s'", *arg);
                ++arg;
            }
            fprintf(stderr, "), exit status %d\n", exit_status);
            result = -exit_status;
            goto cleanup;
        }

        cleanup:
        close(fd[READ_END]);
        return result;
    }
}

int wait_program(pid_t process_id) {
    int status;
    if(waitpid(process_id, &status, WUNTRACED) == -1) {
        perror("waitpid failed");
        return -errno;
    }

    if(!WIFEXITED(status))
        return -4;

    return WEXITSTATUS(status);
}

int exec_program_async(const char **args, pid_t *result_process_id) {
    /* 1 arguments */
    if(args[0] == NULL)
        return -1;

    pid_t pid = fork();
    if(pid == -1) {
        int err = errno;
        perror("Failed to fork");
        return -err;
    } else if(pid == 0) { /* child */
        execvp(args[0], args);
    } else { /* parent */
        if(result_process_id)
            *result_process_id = pid;
    }
    return 0;
}

#if 0
int program_pipe_write(ProgramPipe *self, const char *data, size_t size) {
    ssize_t bytes_written = write(self->write_fd, data, size);
    if(bytes_written == -1) {
        int err = errno;
        perror("Failed to write to pipe to program");
        return -err;
    }
    return 0;
}

int program_pipe_read(ProgramPipe *self, ProgramOutputCallback output_callback, void *userdata) {
    char buffer[2048];

    for(;;) {
        ssize_t bytes_read = read(self->read_fd, buffer, sizeof(buffer) - 1);
        if(bytes_read == 0) {
            break;
        } else if(bytes_read == -1) {
            int err = errno;
            perror("Failed to read from pipe to program");
            return -err;
        }

        buffer[bytes_read] = '\0';
        if(output_callback && output_callback(buffer, bytes_read, userdata) != 0)
            break;
    }

    return 0;
}

void program_pipe_close(ProgramPipe *self) {
    close(self->read_fd);
    close(self->write_fd);
    self->read_fd = -1;
    self->write_fd = -1;
}
#endif
