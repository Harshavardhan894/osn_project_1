#ifndef PARSER_H
#define PARSER_H

#include "command.h"
int is_valid_command(char *input);
char* trim(char *s);
void parse_atomic_command(char *input, command_t *cmd);
void free_command(command_t *cmd);
#endif