#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "command.h"
#include "parser.h"

void execute_command(command_t *cmd) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        if (cmd->input_file != NULL) {
            int fd = open(cmd->input_file, O_RDONLY);
            if (fd < 0) {
                printf("No such file or directory\n");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (cmd->output_file != NULL) {
            int fd;
            if (cmd->append) {
                fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            } else {
                fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }

            if (fd < 0) {
                printf("Unable to create file for writing\n");
                exit(1);
            }

            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        if (cmd->args[0] == NULL) exit(0);

        if (execvp(cmd->args[0], cmd->args) == -1) {
            printf("Command not found!\n");
            exit(1);
        }
    }
    else {
        waitpid(pid, NULL, 0);
    }
}

void execute_pipeline(char *group) {
    char temp[1024];
    strncpy(temp, group, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *parts[100];
    int n = 0;
    char *tok = strtok(temp, "|");
    while (tok != NULL) {
        parts[n++] = trim(tok);
        tok = strtok(NULL, "|");
    }
    if (n == 0) return;

    int pipes[100][2];
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) < 0) { perror("pipe"); return; }
    }

    pid_t pids[100];
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return; }
        pids[i] = pid;

        if (pid == 0) {
            if (i > 0)     dup2(pipes[i-1][0], STDIN_FILENO);
            if (i < n - 1) dup2(pipes[i][1],   STDOUT_FILENO);

            for (int j = 0; j < n - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            command_t cmd;
            parse_atomic_command(parts[i], &cmd);
            if (cmd.input_file != NULL) {
                int fd = open(cmd.input_file, O_RDONLY);
                if (fd < 0) { perror(cmd.input_file); exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (cmd.output_file != NULL) {
                int flags = O_WRONLY | O_CREAT | (cmd.append ? O_APPEND : O_TRUNC);
                int fd = open(cmd.output_file, flags, 0644);
                if (fd < 0) { perror(cmd.output_file); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            if (cmd.args[0] == NULL) exit(0);
            execvp(cmd.args[0], cmd.args);
            perror(cmd.args[0]);
            exit(1);
        }
    }

    for (int i = 0; i < n - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    for (int i = 0; i < n; i++) waitpid(pids[i], NULL, 0);
}