#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "hop.h"
#include "reveal.h"

#define INPUT_BUFFER_SIZE 1024

#define MAX_LOG 15

extern char LOG_PATH[];

void read_log(char logs[MAX_LOG][1024], int *count) {
    FILE *fp = fopen(LOG_PATH, "r");
    *count = 0;
    if (!fp) return;
    while (fgets(logs[*count], 1024, fp)) {
        logs[*count][strcspn(logs[*count], "\n")] = '\0';
        (*count)++;
    }
    fclose(fp);
}

void write_log(char logs[MAX_LOG][1024], int count) {
    FILE *fp = fopen(LOG_PATH, "w");
    if (!fp) return;

    for (int i = 0; i < count; i++) {
        fprintf(fp, "%s\n", logs[i]);
    }

    fclose(fp);
}

void log_add(char *cmd) {

    if (strncmp(cmd, "log", 3) == 0) return;

    char logs[MAX_LOG][1024];
    int count;

    read_log(logs, &count);

    if (count > 0 && strcmp(logs[count - 1], cmd) == 0) return;

    if (count == MAX_LOG) {
        for (int i = 1; i < MAX_LOG; i++) {
            strcpy(logs[i - 1], logs[i]);
        }
        count--;
    }

    strcpy(logs[count++], cmd);

    write_log(logs, count);
}

void log_print() {
    char logs[MAX_LOG][1024];
    int count;

    read_log(logs, &count);

    for (int i = 0; i < count; i++) {
        printf("%d %s\n", i + 1, logs[i]);
    }
}

void log_purge() {
    FILE *fp = fopen(LOG_PATH, "w");
    if (fp) fclose(fp);
}

char* log_get(int index) {
    static char logs[MAX_LOG][1024];
    int count;
    read_log(logs, &count);
    int pos = count - index;
    if (pos < 0 || pos >= count) return NULL;
    return logs[pos];
}

void execute_log(char **tokens, int count) {
    if (count == 1) {
            log_print();
        }
        else if (count == 2 && strcmp(tokens[1], "purge") == 0) {
            log_purge();
        }
        else if (count == 3 && strcmp(tokens[1], "execute") == 0) {
            int idx = atoi(tokens[2]);
            char *cmd = log_get(idx);

            if (!cmd) {
                printf("log: Invalid Syntax!\n");
            } else {
                char temp2[INPUT_BUFFER_SIZE];
                strcpy(temp2, cmd);
                char *toks2[100];
                int c2 = 0;
                char *t = strtok(temp2, " ");
                while (t != NULL) {
                    toks2[c2++] = t;
                    t = strtok(NULL, " ");
                }
                toks2[c2] = NULL;

                if (strcmp(toks2[0], "hop") == 0) {
                    execute_hop(toks2, c2);
                }
                else if (strcmp(toks2[0], "reveal") == 0) {
                    execute_reveal(toks2, c2);
                }
            }
        }
        else {
            printf("log: Invalid Syntax!\n");
        }
    }
