#ifndef EXECUTOR_H
#define EXECUTOR_H
#include "erraid.h"
#include <stdbool.h>
#include <time.h>

bool validate_command(command *cmd, int *total_count);
bool validate_tasks(tasks *T);
bool should_run(tasks *T, struct tm *tm_now);
void execute_task(tasks *T);
int execute_cmd(command *cmd, const char *chemin_stdout, const char *chemin_stderr);

#endif