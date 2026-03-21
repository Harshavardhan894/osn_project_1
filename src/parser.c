#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include "parser.h"
#include "command.h"

#define MAX_TOKENS 100

int split(char *str, char *delim, char **out) {
    int count = 0;
    char *token = strtok(str, delim);
    while (token != NULL) {
        out[count++] = token;
        token = strtok(NULL, delim);
    }
    return count;
}

char* trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

int is_operator(char *t) {
    return strcmp(t, "|") == 0 || strcmp(t, ";") == 0 || strcmp(t, "&") == 0;
}

int valid_atomic(char *cmd) {
    char *tokens[MAX_TOKENS];
    int count = 0;
    char temp[1024];
    strncpy(temp, cmd, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *token = strtok(temp, " \t\n\r");
    while (token != NULL) {
        tokens[count++] = token;
        token = strtok(NULL, " \t\n\r");
    }
    if (count == 0) return 0;
    if (strchr("|&;<>", tokens[0][0])) return 0;

    for (int i = 0; i < count; i++) {
        if (strcmp(tokens[i], "<") == 0 ||
            strcmp(tokens[i], ">") == 0 ||
            strcmp(tokens[i], ">>") == 0) {
            if (i + 1 >= count) return 0;
            if (strchr("|&;<>", tokens[i+1][0])) return 0;
            i++;
        }
    }
    return 1;
}

// KEY FIX: strdup all tokens so they survive beyond this stack frame
void parse_atomic_command(char *input, command_t *cmd) {
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->append = 0;
    for (int k = 0; k < MAX_ARGS; k++) cmd->args[k] = NULL;

    int i = 0;
    char temp[1024];
    strncpy(temp, input, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *token = strtok(temp, " \t\n\r");
    while (token != NULL) {
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " \t\n\r");
            if (token) cmd->input_file = strdup(token); // FIX: strdup
        } else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " \t\n\r");
            if (token) {
                cmd->output_file = strdup(token); // FIX: strdup
                cmd->append = 0;
            }
        } else if (strcmp(token, ">>") == 0) {
            token = strtok(NULL, " \t\n\r");
            if (token) {
                cmd->output_file = strdup(token); // FIX: strdup
                cmd->append = 1;
            }
        } else {
            cmd->args[i++] = strdup(token); // FIX: strdup
        }
        token = strtok(NULL, " \t\n\r");
    }
    cmd->args[i] = NULL;
}

// FIX: free strdup'd memory after a command is done
void free_command(command_t *cmd) {
    for (int i = 0; i < MAX_ARGS && cmd->args[i]; i++) {
        free(cmd->args[i]);
        cmd->args[i] = NULL;
    }
    if (cmd->input_file)  { free(cmd->input_file);  cmd->input_file  = NULL; }
    if (cmd->output_file) { free(cmd->output_file); cmd->output_file = NULL; }
}

int valid_cmd_group(char *group) {
    char *parts[MAX_TOKENS];
    int count = 0;
    char temp[1024];
    strncpy(temp, group, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    count = split(temp, "|", parts);
    if (count == 0) return 0;
    for (int i = 0; i < count; i++) {
        char *part = trim(parts[i]);
        if (strlen(part) == 0) return 0;
        if (!valid_atomic(part)) return 0;
    }
    return 1;
}

int is_valid_command(char *input) {
    char temp[1024];
    strncpy(temp, input, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    char *groups[MAX_TOKENS];
    int count = 0;
    char *token = strtok(temp, ";&");
    while (token != NULL) {
        groups[count++] = token;
        token = strtok(NULL, ";&");
    }
    if (count == 0) return 0;
    for (int i = 0; i < count; i++) {
        char *group = trim(groups[i]);
        if (strlen(group) == 0) return 0;
        if (!valid_cmd_group(group)) return 0;
    }
    return 1;
}