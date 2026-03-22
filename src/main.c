#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <termios.h>

#include "parser.h"
#include "hop.h"
#include "reveal.h"
#include "log.h"
#include "executor.h"
#include "command.h"
#include "prompt.h"

#define INPUT_BUFFER_SIZE 1024  
#define MAX_BG_JOBS 256
#define MAX_CMD_HISTORY 200

char HOME_DIR[PATH_MAX];
char LOG_PATH[PATH_MAX];

typedef struct {
    int active;
    int job_number;
    pid_t pid;
    char command_name[128];
    int is_stopped;
} background_job_t;

static background_job_t bg_jobs[MAX_BG_JOBS];
static int next_job_number = 1;
static char fg_command_name[128] = "";
static char cmd_history[MAX_CMD_HISTORY][INPUT_BUFFER_SIZE];
static int cmd_history_count = 0;

static void handle_sigint(int signo) {
    (void)signo;
    signal_foreground_process_group(SIGINT);
}

static void handle_sigtstp(int signo) {
    (void)signo;
    signal_foreground_process_group(SIGTSTP);
}

static void install_signal_handlers(void) {
    struct sigaction sa_int;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sa_int.sa_flags = SA_RESTART;
    sigemptyset(&sa_int.sa_mask);
    sigaction(SIGINT, &sa_int, NULL);

    struct sigaction sa_tstp;
    memset(&sa_tstp, 0, sizeof(sa_tstp));
    sa_tstp.sa_handler = handle_sigtstp;
    sa_tstp.sa_flags = SA_RESTART;
    sigemptyset(&sa_tstp.sa_mask);
    sigaction(SIGTSTP, &sa_tstp, NULL);
}

static int parse_long_strict(const char *s, long *out) {
    if (s == NULL || *s == '\0') return 0;

    char *end = NULL;
    errno = 0;
    long value = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') return 0;

    *out = value;
    return 1;
}

static void build_prompt(char *out, size_t out_size) {
    char cwd[PATH_MAX];
    char hostname[256];
    char *username = getenv("USER");

    if (username == NULL) username = "user";
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "host", sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
    }
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strncpy(cwd, "?", sizeof(cwd) - 1);
        cwd[sizeof(cwd) - 1] = '\0';
    }

    if (strncmp(cwd, HOME_DIR, strlen(HOME_DIR)) == 0) {
        if (strlen(cwd) == strlen(HOME_DIR)) {
            snprintf(out, out_size, "<%s@%s:~> ", username, hostname);
        } else {
            snprintf(out, out_size, "<%s@%s:~%s> ", username, hostname, cwd + strlen(HOME_DIR));
        }
    } else {
        snprintf(out, out_size, "<%s@%s:%s> ", username, hostname, cwd);
    }
}

static void add_input_history(const char *line) {
    if (line == NULL || line[0] == '\0') return;
    if (cmd_history_count > 0 && strcmp(cmd_history[cmd_history_count - 1], line) == 0) return;

    if (cmd_history_count == MAX_CMD_HISTORY) {
        for (int i = 1; i < MAX_CMD_HISTORY; i++) {
            strcpy(cmd_history[i - 1], cmd_history[i]);
        }
        cmd_history_count--;
    }

    strncpy(cmd_history[cmd_history_count], line, INPUT_BUFFER_SIZE - 1);
    cmd_history[cmd_history_count][INPUT_BUFFER_SIZE - 1] = '\0';
    cmd_history_count++;
}

static void redraw_input_line(const char *prompt, const char *line) {
    printf("\33[2K\r%s%s", prompt, line);
    fflush(stdout);
}

/* returns 1 on success, 0 on EOF (Ctrl-D), -1 on read error */
static int read_input_line(char *out, size_t out_size) {
    char prompt[PATH_MAX + 300];
    build_prompt(prompt, sizeof(prompt));
    printf("%s", prompt);
    fflush(stdout);

    struct termios oldt;
    struct termios newt;

    if (tcgetattr(STDIN_FILENO, &oldt) < 0) return -1;
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);

    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) < 0) return -1;

    size_t len = 0;
    int hist_index = cmd_history_count;
    out[0] = '\0';

    while (1) {
        char c;
        ssize_t r = read(STDIN_FILENO, &c, 1);
        if (r <= 0) {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            return -1;
        }

        if (c == '\n' || c == '\r') {
            out[len] = '\0';
            printf("\n");
            fflush(stdout);
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            return 1;
        }

        if (c == 4) {
            if (len == 0) {
                tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
                return 0;
            }
            continue;
        }

        if (c == 127 || c == 8) {
            if (len > 0) {
                len--;
                out[len] = '\0';
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }

        if (c == 27) {
            char seq1, seq2;
            if (read(STDIN_FILENO, &seq1, 1) <= 0 || read(STDIN_FILENO, &seq2, 1) <= 0) {
                continue;
            }

            if (seq1 == '[' && seq2 == 'A') {
                if (cmd_history_count > 0 && hist_index > 0) {
                    hist_index--;
                    strncpy(out, cmd_history[hist_index], out_size - 1);
                    out[out_size - 1] = '\0';
                    len = strlen(out);
                    redraw_input_line(prompt, out);
                }
            } else if (seq1 == '[' && seq2 == 'B') {
                if (hist_index < cmd_history_count - 1) {
                    hist_index++;
                    strncpy(out, cmd_history[hist_index], out_size - 1);
                    out[out_size - 1] = '\0';
                    len = strlen(out);
                    redraw_input_line(prompt, out);
                } else if (hist_index == cmd_history_count - 1) {
                    hist_index = cmd_history_count;
                    out[0] = '\0';
                    len = 0;
                    redraw_input_line(prompt, out);
                }
            }
            continue;
        }

        if (isprint((unsigned char)c)) {
            if (len + 1 < out_size) {
                out[len++] = c;
                out[len] = '\0';
                putchar(c);
                fflush(stdout);
            }
        }
    }
}

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

static int add_background_job(pid_t pid, const char *command_name, int is_stopped, int print_spawn_message) {
    for (int i = 0; i < MAX_BG_JOBS; i++) {
        if (!bg_jobs[i].active) {
            bg_jobs[i].active = 1;
            bg_jobs[i].job_number = next_job_number++;
            bg_jobs[i].pid = pid;
            bg_jobs[i].is_stopped = is_stopped;
            strncpy(bg_jobs[i].command_name, command_name, sizeof(bg_jobs[i].command_name) - 1);
            bg_jobs[i].command_name[sizeof(bg_jobs[i].command_name) - 1] = '\0';
            if (print_spawn_message) {
                printf("[%d] %d\n", bg_jobs[i].job_number, (int)pid);
            }
            return bg_jobs[i].job_number;
        }
    }
    return -1;
}

static int compare_jobs_by_name(const void *a, const void *b) {
    const background_job_t *ja = *(const background_job_t * const *)a;
    const background_job_t *jb = *(const background_job_t * const *)b;

    int cmp = strcmp(ja->command_name, jb->command_name);
    if (cmp != 0) return cmp;
    if (ja->pid < jb->pid) return -1;
    if (ja->pid > jb->pid) return 1;
    return 0;
}

static void print_activities(void) {
    background_job_t *active_jobs[MAX_BG_JOBS];
    int count = 0;

    for (int i = 0; i < MAX_BG_JOBS; i++) {
        if (bg_jobs[i].active) {
            active_jobs[count++] = &bg_jobs[i];
        }
    }

    qsort(active_jobs, count, sizeof(active_jobs[0]), compare_jobs_by_name);

    for (int i = 0; i < count; i++) {
        printf("[%d] : %s - %s\n",
               (int)active_jobs[i]->pid,
               active_jobs[i]->command_name,
               active_jobs[i]->is_stopped ? "Stopped" : "Running");
    }
}

static void check_background_processes(void) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        for (int i = 0; i < MAX_BG_JOBS; i++) {
            if (bg_jobs[i].active && bg_jobs[i].pid == pid) {
                if (WIFEXITED(status)) {
                    if (WEXITSTATUS(status) == 0) {
                        printf("%s with pid %d exited normally\n", bg_jobs[i].command_name, (int)pid);
                    } else {
                        printf("%s with pid %d exited abnormally\n", bg_jobs[i].command_name, (int)pid);
                    }
                    bg_jobs[i].active = 0;
                } else if (WIFSIGNALED(status)) {
                    printf("%s with pid %d exited abnormally\n", bg_jobs[i].command_name, (int)pid);
                    bg_jobs[i].active = 0;
                } else if (WIFSTOPPED(status)) {
                    bg_jobs[i].is_stopped = 1;
                } else if (WIFCONTINUED(status)) {
                    bg_jobs[i].is_stopped = 0;
                }
                break;
            }
        }
    }
}

int main() {
    char input[INPUT_BUFFER_SIZE];

    install_signal_handlers();

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
        int read_status = read_input_line(input, sizeof(input));
        if (read_status == -1) {
            continue;
        }
        if (read_status == 0) {
            for (int i = 0; i < MAX_BG_JOBS; i++) {
                if (bg_jobs[i].active) {
                    kill(-bg_jobs[i].pid, SIGKILL);
                }
            }
            pid_t fg = get_foreground_pgid();
            if (fg > 0) {
                kill(-fg, SIGKILL);
            }
            printf("logout\n");
            return 0;
        }

        add_input_history(input);

        check_background_processes();

        if (!is_valid_command(input)) {
            printf("Invalid Syntax!\n");
            continue;
        }
        log_add(input);
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
                char command_name[128];
                extract_command_name(group_trim, command_name, sizeof(command_name));

                if (run_in_background) {
                    pid_t pid = execute_pipeline_background(group_trim);
                    if (pid > 0) (void)add_background_job(pid, command_name, 0, 1);
                } else {
                    pid_t pgid = 0;
                    int stopped = 0;

                    strncpy(fg_command_name, command_name, sizeof(fg_command_name) - 1);
                    fg_command_name[sizeof(fg_command_name) - 1] = '\0';

                    if (execute_pipeline_foreground(group_trim, &pgid, &stopped) == 0) {
                        if (stopped && pgid > 0) {
                            int job_no = add_background_job(pgid, fg_command_name, 1, 0);
                            if (job_no > 0) {
                                printf("[%d] Stopped %s\n", job_no, fg_command_name);
                            }
                        }
                    }
                    fg_command_name[0] = '\0';
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
            if (strcmp(tokens[0], "activities") == 0) {
                print_activities();
                continue;
            }
            if (strcmp(tokens[0], "ping") == 0) {
                if (count != 3) {
                    printf("Invalid syntax!\n");
                    continue;
                }

                long pid_value;
                long signal_number;
                if (!parse_long_strict(tokens[1], &pid_value) || !parse_long_strict(tokens[2], &signal_number)) {
                    printf("Invalid syntax!\n");
                    continue;
                }

                int actual_signal = (int)(signal_number % 32);
                if (actual_signal < 0) actual_signal += 32;

                if (kill((pid_t)pid_value, actual_signal) < 0) {
                    if (errno == ESRCH) {
                        printf("No such process found\n");
                    } else {
                        printf("No such process found\n");
                    }
                } else {
                    printf("Sent signal %ld to process with pid %ld\n", signal_number, pid_value);
                }
                continue;
            }

            command_t cmd;
            parse_atomic_command(group_trim, &cmd);
            if (run_in_background) {
                pid_t pid = execute_command_background(&cmd);
                if (pid > 0) (void)add_background_job(pid, cmd.args[0] ? cmd.args[0] : "command", 0, 1);
            } else {
                pid_t pgid = 0;
                int stopped = 0;

                strncpy(fg_command_name, cmd.args[0] ? cmd.args[0] : "command", sizeof(fg_command_name) - 1);
                fg_command_name[sizeof(fg_command_name) - 1] = '\0';

                if (execute_command_foreground(&cmd, &pgid, &stopped) == 0) {
                    if (stopped && pgid > 0) {
                        int job_no = add_background_job(pgid, fg_command_name, 1, 0);
                        if (job_no > 0) {
                            printf("[%d] Stopped %s\n", job_no, fg_command_name);
                        }
                    }
                }
                fg_command_name[0] = '\0';
            }
            free_command(&cmd);
        }
    }

    return 0;
}