#ifndef QUICKMEDIA_PROGRAM_H
#define QUICKMEDIA_PROGRAM_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    pid_t pid;
    int read_fd;
    int write_fd;
} ProgramPipe;

/* Return 0 if you want to continue reading. @data is null-terminated */
typedef int (*ProgramOutputCallback)(char *data, int size, void *userdata);

/*
    @args need to have at least 2 arguments. The first which is the program name
    and the last which is NULL, which indicates end of args
*/
int exec_program(const char **args, ProgramOutputCallback output_callback, void *userdata);

// Return the exit status, or a negative value if waiting failed
int wait_program(pid_t process_id);

/*
    @args need to have at least 2 arguments. The first which is the program name
    and the last which is NULL, which indicates end of args
*/
int exec_program_async(const char **args, pid_t *result_process_id);
#if 0

int program_pipe_write(ProgramPipe *self, const char *data, size_t size);
int program_pipe_read(ProgramPipe *self, ProgramOutputCallback output_callback, void *userdata);
void program_pipe_close(ProgramPipe *self);
#endif

#ifdef __cplusplus
}
#endif

#endif
