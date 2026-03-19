#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

#include "parser.c"
#include "hop.c"
#include "reveal.c"
#include "log.c"

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
        char temp[INPUT_BUFFER_SIZE];
        strcpy(temp, input);
        char *tokens[100];
        int count = 0;
        char *token = strtok(temp, " ");
        while (token != NULL) {
            tokens[count++] = token;
            token = strtok(NULL, " ");
        }
        tokens[count] = NULL;

        if (count == 0) continue;

        if (strcmp(tokens[0], "log") == 0) {

            execute_log(tokens, count);
            continue;
        }

        if (strcmp(tokens[0], "hop") == 0) {
            execute_hop(tokens, count);
            continue;
        }

        if (strcmp(tokens[0], "reveal") == 0) {
            execute_reveal(tokens, count);
            continue;
        }
    }

    return 0;
}