#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "command.h"
#include <sys/types.h>

void execute_command(command_t *cmd);
void execute_pipeline(char *group);
int execute_command_foreground(command_t *cmd, pid_t *pgid_out, int *stopped_out);
int execute_pipeline_foreground(char *group, pid_t *pgid_out, int *stopped_out);
pid_t execute_command_background(command_t *cmd);
pid_t execute_pipeline_background(char *group);
pid_t get_foreground_pgid(void);
void signal_foreground_process_group(int sig);

#endif