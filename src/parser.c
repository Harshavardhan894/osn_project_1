#include <string.h>
#include <ctype.h>
#include "parser.h"

int skip_spaces(char *s, int i) {
    while (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r') {
        i++;
    }
    return i;
}

int is_valid_command(char *input) {
    int i = 0;
    int len = strlen(input);

    i = skip_spaces(input, i);

    if (input[i] == '|' || input[i] == ';' || input[i] == '&') {
        return 0;
    }

    for (; i < len; i++) {
        if (isspace(input[i])) continue;
        if (input[i] == '|') {
            int j = skip_spaces(input, i + 1);

            if (j >= len || input[j] == '|' || input[j] == ';' || input[j] == '&') {
                return 0;
            }
        }

        if (input[i] == ';') {
            int j = skip_spaces(input, i + 1);

            if (j >= len || input[j] == ';' || input[j] == '|') {
                return 0;
            }
        }
        if (input[i] == '&') {
            int j = skip_spaces(input, i + 1);

            if (j < len && (input[j] == '&' || input[j] == '|' || input[j] == ';')) {
                return 0;
            }
        }
    }
    i = len - 1;
    while (i >= 0 && isspace(input[i])) i--;

    if (i >= 0 && (input[i] == '|' || input[i] == ';')) {
        return 0;
    }

    return 1;
}