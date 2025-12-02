#ifndef TASKS_IO_H
#define TASKS_IO_H

#include "erraid.h"
#include <stdint.h>


uint64_t number_of_tasks(char *chemin);
tasks *read_tasks(char *chemin);
void free_tasks(tasks *T, uint64_t count); 

#endif