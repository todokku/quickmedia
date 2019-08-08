#ifndef QUICKMEDIA_PROGRAM_H
#define QUICKMEDIA_PROGRAM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Return 0 if you want to continue reading. @data is null-terminated */
typedef int (*ProgramOutputCallback)(char *data, int size, void *userdata);

/*
    @args need to have at least 2 arguments. The first which is the program name
    and the last which is NULL, which indicates end of args
*/
int exec_program(const char **args, ProgramOutputCallback output_callback, void *userdata);

#ifdef __cplusplus
}
#endif

#endif
