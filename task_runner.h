#ifndef TASK_RUNNER_H
#define TASK_RUNNER_H

#include "erraid.h"
#include <time.h>
#include <stdbool.h>


bool should_run(tasks *T, struct tm *tm_now);
void log(tasks *T, int exit_code) ; 
void execute_task(tasks *T);

#endif
