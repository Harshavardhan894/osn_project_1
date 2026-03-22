#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include "command.h"
#include "parser.h"

static volatile sig_atomic_t current_fg_pgid = 0;

pid_t get_foreground_pgid(void) {
    return (pid_t)current_fg_pgid;
}

void signal_foreground_process_group(int sig) {
    if (current_fg_pgid > 0) {
        kill(-current_fg_pgid, sig);
    }
}

static void execute_in_child(command_t *cmd, int detach_stdin) {
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);

    if (cmd->input_file != NULL) {
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0) {
            printf("No such file or directory\n");
            exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    } else if (detach_stdin) {
        int fd = open("/dev/null", O_RDONLY);
        if (fd >= 0) {
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
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

int execute_command_foreground(command_t *cmd, pid_t *pgid_out, int *stopped_out) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return -1;
    }

    if (pid == 0) {
        if (setpgid(0, 0) < 0) {
            perror("setpgid");
            exit(1);
        }
        execute_in_child(cmd, 0);
    }

    if (setpgid(pid, pid) < 0 && errno != EACCES) {
        perror("setpgid");
    }

    if (pgid_out) *pgid_out = pid;
    current_fg_pgid = pid;

    int status;
    while (waitpid(pid, &status, WUNTRACED) < 0) {
        if (errno != EINTR) {
            current_fg_pgid = 0;
            return -1;
        }
    }
    current_fg_pgid = 0;

    if (stopped_out) {
        *stopped_out = WIFSTOPPED(status) ? 1 : 0;
    }

    return 0;
}

void execute_command(command_t *cmd) {
    (void)execute_command_foreground(cmd, NULL, NULL);
}

pid_t execute_command_background(command_t *cmd) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return -1;
    }

    if (pid == 0) {
        if (setpgid(0, 0) < 0) {
            perror("setpgid");
            exit(1);
        }
        execute_in_child(cmd, 1);
    }

    if (setpgid(pid, pid) < 0 && errno != EACCES) {
        perror("setpgid");
    }

    return pid;
}

static int spawn_pipeline(char *group, int background, pid_t *pgid_out, int *stopped_out) {
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
    if (n == 0) return -1;

    int pipes[100][2];
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) < 0) { perror("pipe"); return -1; }
    }

    pid_t pgid = 0;

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return -1;
        }
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            if (i == 0) {
                if (setpgid(0, 0) < 0) {
                    perror("setpgid");
                    exit(1);
                }
            } else {
                if (setpgid(0, pgid) < 0) {
                    perror("setpgid");
                    exit(1);
                }
            }

            if (i > 0)     dup2(pipes[i-1][0], STDIN_FILENO);
            if (i < n - 1) dup2(pipes[i][1],   STDOUT_FILENO);

            if (background && i == 0) {
                int fd = open("/dev/null", O_RDONLY);
                if (fd >= 0) {
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
            }

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

        if (i == 0) pgid = pid;
        if (setpgid(pid, pgid) < 0 && errno != EACCES) {
            perror("setpgid");
        }
    }

    for (int i = 0; i < n - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    if (pgid_out) *pgid_out = pgid;
    if (stopped_out) *stopped_out = 0;

    if (background) {
        return 0;
    }

    current_fg_pgid = pgid;

    int alive = n;
    while (alive > 0) {
        int status;
        pid_t w = waitpid(-pgid, &status, WUNTRACED);
        if (w < 0) {
            if (errno == EINTR) continue;
            current_fg_pgid = 0;
            break;
        }
        if (WIFSTOPPED(status)) {
            if (stopped_out) *stopped_out = 1;
            break;
        }
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            alive--;
        }
    }

    if (stopped_out && *stopped_out == 0) {
        while (alive > 0) {
            int status;
            pid_t w = waitpid(-pgid, &status, 0);
            if (w < 0) {
                if (errno == EINTR) continue;
                current_fg_pgid = 0;
                break;
            }
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                alive--;
            }
        }
    }

    current_fg_pgid = 0;

    return 0;
}

int execute_pipeline_foreground(char *group, pid_t *pgid_out, int *stopped_out) {
    return spawn_pipeline(group, 0, pgid_out, stopped_out);
}

void execute_pipeline(char *group) {
    (void)spawn_pipeline(group, 0, NULL, NULL);
}

pid_t execute_pipeline_background(char *group) {
    pid_t pgid = -1;
    if (spawn_pipeline(group, 1, &pgid, NULL) < 0) {
        return -1;
    }
    return pgid;
}