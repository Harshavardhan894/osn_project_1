#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "command.h"
#include <sys/types.h>

void execute_command(command_t *cmd);
void execute_pipeline(char *group);
pid_t execute_command_background(command_t *cmd);
pid_t execute_pipeline_background(char *group);

#endif