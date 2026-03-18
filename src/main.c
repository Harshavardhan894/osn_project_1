#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

#include "prompt.h"
#include "parser.h"
#include "parser.c"
#define INPUT_BUFFER_SIZE 1024

char HOME_DIR[PATH_MAX];

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
        }
        else {
            printf("<%s@%s:~%s> ", username, hostname, cwd + strlen(HOME_DIR));
        }
    } else {
        printf("<%s@%s:%s> ", username, hostname, cwd);
    }
}

int main() {
    char input[INPUT_BUFFER_SIZE];

    getcwd(HOME_DIR, sizeof(HOME_DIR));
    while (1) {
        print_prompt();
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\n");
            break;
        }
        input[strcspn(input, "\n")] = '\0';
        if (!is_valid_command(input)) {
            printf("Invalid Syntax!\n");
        }
    }

    return 0;
}