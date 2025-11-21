#ifndef LECTEUR_H
#define LECTEUR_H
#include "erraid.h"

uint64_t number_of_tasks(char *chemin);
void read_cmd(command *c, char *chemin, char *chemin_cmd);
tasks *read_tasks(char *chemin);
void free_tasks(tasks *T, uint64_t count);
void free_cmd(command *cmd);
void print_command(const command *cmd);

#endif