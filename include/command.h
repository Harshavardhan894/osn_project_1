#ifndef COMMAND_H
#define COMMAND_H

#define MAX_ARGS 100

typedef struct {
    char *args[MAX_ARGS];
    char *input_file;
    char *output_file;
    int append;
} command_t;

#endif