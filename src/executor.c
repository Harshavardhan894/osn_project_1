#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "command.h"
#include "executor.h"

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
                fd = open(cmd->output_file,
                          O_WRONLY | O_CREAT | O_APPEND,
                          0644);
            } else {
                fd = open(cmd->output_file,
                          O_WRONLY | O_CREAT | O_TRUNC,
                          0644);
            }

            if (fd < 0) {
                printf("Unable to create file for writing\n");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        if (execvp(cmd->args[0], cmd->args) == -1) {
            printf("Command not found!\n");
            exit(1);
        }
    } else {
        waitpid(pid, NULL, 0);
    }
}