#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "parser.h"

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
    while (isspace(*s)) s++;
    return s;
}

int is_operator(char *t) {
    return strcmp(t, "|") == 0 || strcmp(t, ";") == 0 || strcmp(t, "&") == 0;
}

int valid_atomic(char *cmd) {
    char *tokens[MAX_TOKENS];
    int count = 0;

    char temp[1024];
    strcpy(temp, cmd);

    char *token = strtok(temp, " \t\n\r");
    while (token != NULL) {
        tokens[count++] = token;
        token = strtok(NULL, " \t\n\r");
    }

    if (count == 0) return 0;

    if (strchr("|&;<>", tokens[0][0])) return 0;

    for (int i = 0; i < count; i++) {

        if (strcmp(tokens[i], "<") == 0) {
            if (i + 1 >= count) return 0;
            if (strchr("|&;<>", tokens[i+1][0])) return 0;
            i++;
        }

        else if (strcmp(tokens[i], ">") == 0) {
            if (i + 1 >= count) return 0;
            if (strchr("|&;<>", tokens[i+1][0])) return 0;
            i++;
        }

        else if (strcmp(tokens[i], ">>") == 0) {
            if (i + 1 >= count) return 0;
            if (strchr("|&;<>", tokens[i+1][0])) return 0;
            i++;
        }

    }

    return 1;
}

int valid_cmd_group(char *group) {
    char *parts[MAX_TOKENS];
    int count = 0;

    char temp[1024];
    strcpy(temp, group);

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
    strcpy(temp, input);

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