#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

#include "prompt.h"
#include "parser.h"
#include "parser.c"
#include "hop.h"
#include "hop.c"

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
        char *tokens[100];
        int count = 0;
        if(strcmp(input,"exit")==0){
            break;
        }
        char *token = strtok(input, " ");
        while (token != NULL) {
            tokens[count++] = token;
            token = strtok(NULL, " ");
        }
        tokens[count] = NULL;
        if (count>0 && strcmp(tokens[0], "hop") == 0) {
            execute_hop(tokens, count);
        }
    }
    return 0;
}