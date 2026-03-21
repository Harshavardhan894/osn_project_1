#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

#include "parser.h"
#include "hop.h"
#include "reveal.h"
#include "log.h"
#include "executor.h"
#include "command.h"
#include "prompt.h"

#define INPUT_BUFFER_SIZE 1024

char HOME_DIR[PATH_MAX];
char LOG_PATH[PATH_MAX];

void print_prompt() {
    char cwd[PATH_MAX];
    char hostname[256];
    char *username;

    username = getenv("USER");
    gethostname(hostname, sizeof(hostname));
    getcwd(cwd, sizeof(cwd));

    if (strncmp(cwd, HOME_DIR, strlen(HOME_DIR)) == 0) {
        if (strlen(cwd) == strlen(HOME_DIR)) {
            printf("<%s@%s:~> ", username, hostname);
        } else {
            printf("<%s@%s:~%s> ", username, hostname, cwd + strlen(HOME_DIR));
        }
    } else {
        printf("<%s@%s:%s> ", username, hostname, cwd);
    }
}
int main() {
    char input[INPUT_BUFFER_SIZE];

    if (getcwd(HOME_DIR, sizeof(HOME_DIR)) == NULL) {
        perror("getcwd");
        return 1;
    }

    size_t len = strlen(HOME_DIR);
    const char *suffix = "/.shell_log";

    if (len + strlen(suffix) + 1 > sizeof(LOG_PATH)) {
        fprintf(stderr, "Path too long\n");
        return 1;
    }

    memcpy(LOG_PATH, HOME_DIR, len);
    memcpy(LOG_PATH + len, suffix, strlen(suffix) + 1);

    while (1) {

        print_prompt();

        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\n");
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        if (!is_valid_command(input)) {
            printf("Invalid Syntax!\n");
            continue;
        }
        log_add(input);
        if (strcmp(input, "exit") == 0) {
            break;
        }
        char temp1[INPUT_BUFFER_SIZE];
        strcpy(temp1, input);

        char *saveptr = NULL;
        char *group = strtok_r(temp1, ";&", &saveptr);
        while (group != NULL) {
            char *group_trim = trim(group);
            if (*group_trim == '\0') {
                group = strtok_r(NULL, ";&", &saveptr);
                continue;
            }

            if (strchr(group_trim, '|') != NULL) {
                execute_pipeline(group_trim);
                group = strtok_r(NULL, ";&", &saveptr);
                continue;
            }

            char temp2[INPUT_BUFFER_SIZE];
            strcpy(temp2, group_trim);

            char *tokens[100];
            int count = 0;

            char *token = strtok(temp2, " \t\n");
            while (token != NULL) {
                tokens[count++] = token;
                token = strtok(NULL, " \t\n");
            }
            tokens[count] = NULL;

            if (count == 0) {
                group = strtok_r(NULL, ";&", &saveptr);
                continue;
            }

            if (strcmp(tokens[0], "log") == 0) {
                execute_log(tokens, count);
                group = strtok_r(NULL, ";&", &saveptr);
                continue;
            }
            if (strcmp(tokens[0], "hop") == 0) {
                execute_hop(tokens, count);
                group = strtok_r(NULL, ";&", &saveptr);
                continue;
            }
            if (strcmp(tokens[0], "reveal") == 0) {
                execute_reveal(tokens, count);
                group = strtok_r(NULL, ";&", &saveptr);
                continue;
            }

            command_t cmd;
            parse_atomic_command(group_trim, &cmd);
            execute_command(&cmd);
            free_command(&cmd);

            group = strtok_r(NULL, ";&", &saveptr);
        }
    }

    return 0;
}