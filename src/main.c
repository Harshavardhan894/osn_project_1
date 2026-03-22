#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <ctype.h>

#include "parser.h"
#include "hop.h"
#include "reveal.h"
#include "log.h"
#include "executor.h"
#include "command.h"
#include "prompt.h"

#define INPUT_BUFFER_SIZE 1024  
#define MAX_BG_JOBS 256

char HOME_DIR[PATH_MAX];
char LOG_PATH[PATH_MAX];

typedef struct {
    int active;
    int job_number;
    pid_t pid;
    char command_name[128];
} background_job_t;

static background_job_t bg_jobs[MAX_BG_JOBS];
static int next_job_number = 1;

static void extract_command_name(const char *group, char *out, size_t out_size) {
    size_t i = 0;

    while (*group && isspace((unsigned char)*group)) group++;

    while (group[i] != '\0' &&
           !isspace((unsigned char)group[i]) &&
           group[i] != '|' && group[i] != '<' && group[i] != '>') {
        if (i + 1 >= out_size) break;
        out[i] = group[i];
        i++;
    }
    out[i] = '\0';

    if (out[0] == '\0') {
        strncpy(out, "command", out_size - 1);
        out[out_size - 1] = '\0';
    }
}

static void add_background_job(pid_t pid, const char *command_name) {
    for (int i = 0; i < MAX_BG_JOBS; i++) {
        if (!bg_jobs[i].active) {
            bg_jobs[i].active = 1;
            bg_jobs[i].job_number = next_job_number++;
            bg_jobs[i].pid = pid;
            strncpy(bg_jobs[i].command_name, command_name, sizeof(bg_jobs[i].command_name) - 1);
            bg_jobs[i].command_name[sizeof(bg_jobs[i].command_name) - 1] = '\0';
            printf("[%d] %d\n", bg_jobs[i].job_number, (int)pid);
            return;
        }
    }
}

static void check_background_processes(void) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < MAX_BG_JOBS; i++) {
            if (bg_jobs[i].active && bg_jobs[i].pid == pid) {
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    printf("%s with pid %d exited normally\n", bg_jobs[i].command_name, (int)pid);
                } else {
                    printf("%s with pid %d exited abnormally\n", bg_jobs[i].command_name, (int)pid);
                }
                bg_jobs[i].active = 0;
                break;
            }
        }
    }
}

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
        check_background_processes();

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

        char *cursor = temp1;
        while (*cursor != '\0') {
            char *group_start = cursor;
            while (*cursor != '\0' && *cursor != ';' && *cursor != '&') cursor++;

            char separator = *cursor;
            if (*cursor != '\0') {
                *cursor = '\0';
                cursor++;
            }

            char *group_trim = trim(group_start);
            if (*group_trim == '\0') {
                continue;
            }

            int run_in_background = (separator == '&');

            if (strchr(group_trim, '|') != NULL) {
                if (run_in_background) {
                    char command_name[128];
                    extract_command_name(group_trim, command_name, sizeof(command_name));
                    pid_t pid = execute_pipeline_background(group_trim);
                    if (pid > 0) add_background_job(pid, command_name);
                } else {
                    execute_pipeline(group_trim);
                }
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
                continue;
            }

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

            command_t cmd;
            parse_atomic_command(group_trim, &cmd);
            if (run_in_background) {
                pid_t pid = execute_command_background(&cmd);
                if (pid > 0) add_background_job(pid, cmd.args[0] ? cmd.args[0] : "command");
            } else {
                execute_command(&cmd);
            }
            free_command(&cmd);
        }
    }

    return 0;
}