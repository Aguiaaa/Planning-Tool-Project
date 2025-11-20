#ifndef TASK_RUNNER_H
#define TASK_RUNNER_H

#include "erraid.h"
#include <time.h>
#include <stdbool.h>

//bool validate_command(command *cmd);
//bool validate_tasks(tasks *T);
bool should_run(tasks *T, struct tm *tm_now);
void execute_task(tasks *T);

#endif
