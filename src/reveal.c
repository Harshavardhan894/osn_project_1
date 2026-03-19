#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>

#include "reveal.h"

extern char HOME_DIR[];
extern char PREV_DIR[];

int compare(const void *a, const void *b) {
    return strcmp(*(char **)a, *(char **)b);
}

void execute_reveal(char **args, int count) {
    int show_all = 0;
    int long_format = 0;

    char *path = NULL;

    for (int i = 1; i < count; i++) {

        if (args[i][0] == '-') {
            for (int j = 1; args[i][j]; j++) {
                if (args[i][j] == 'a') show_all = 1;
                else if (args[i][j] == 'l') long_format = 1;
            }
        } else {
            if (path != NULL) {
                printf("reveal: Invalid Syntax!\n");
                return;
            }
            path = args[i];
        }
    }

    char target[PATH_MAX];

    if (path == NULL ||strcmp(path, ".") == 0) {
        getcwd(target, sizeof(target));
    }
    else if (strcmp(path, "~") == 0) {
        strcpy(target, HOME_DIR);
    }
    else if (strcmp(path, "..") == 0) {
        strcpy(target, "..");
    }
    else if (strcmp(path, "-") == 0) {
        if (strlen(PREV_DIR) == 0) {
            printf("No such directory!\n");
            return;
        }
        strcpy(target, PREV_DIR);
    }
    else {
        strcpy(target, path);
    }

    DIR *dir = opendir(target);
    if (!dir) {
        printf("No such directory!\n");
        return;
    }

    char *files[1024];
    int count_files = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (!show_all && entry->d_name[0] == '.') continue;
        files[count_files++] = strdup(entry->d_name);
    }

    closedir(dir);

    qsort(files, count_files, sizeof(char *), compare);

    for (int i = 0; i < count_files; i++) {
        printf("%s", files[i]);

        if (long_format) printf("\n");
        else printf(" ");

        free(files[i]);
    }

    if (!long_format) printf("\n");
}