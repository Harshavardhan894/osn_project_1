#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "command.h"

void execute_command(command_t *cmd);
void execute_pipeline(char *group);

#endif