#ifndef LOG_H
#define LOG_H

void log_add(char *cmd);
void log_print();
void log_purge();
char* log_get(int index);
void execute_log(char**args, int count);
#endif