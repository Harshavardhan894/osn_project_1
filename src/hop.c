#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <hop.h>

extern char HOME_DIR[];

static char PREV_DIR[PATH_MAX] = "";

void execute_hop(char **args, int count) {
    char current[PATH_MAX];
    getcwd(current, sizeof(current));
    if (count == 1) {
        chdir(HOME_DIR);
        strcpy(PREV_DIR, current);
        return;
    }

    for (int i = 1; i < count; i++) {
        char next_dir[PATH_MAX];
        getcwd(current, sizeof(current));

        if (strcmp(args[i], "~") == 0) {
            strcpy(next_dir, HOME_DIR);
        }
        else if (strcmp(args[i], ".") == 0) {
            continue;
        }
        else if (strcmp(args[i], "..") == 0) {
            strcpy(next_dir, "..");
        }
        else if (strcmp(args[i], "-") == 0) {
            if (strlen(PREV_DIR) == 0) continue;
            strcpy(next_dir, PREV_DIR);
        }
        else {
            strcpy(next_dir, args[i]);
        }

        if (chdir(next_dir) != 0) {
            printf("No such directory!\n");
        } else {
            strcpy(PREV_DIR, current);
        }
    }
}