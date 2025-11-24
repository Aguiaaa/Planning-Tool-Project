#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "scheduler.h"

int main(int argc, char *argv[]) {
    printf("Lancement du programme.\n");
    char chemin_aux[256];
    char *chemin_tasks;
    if (argc == 1) {
        char *user = getenv("USER");
        snprintf(chemin_aux, sizeof chemin_aux, "/tmp/%s/erraid", user);
        chemin_tasks = chemin_aux;
    } else if (argc == 3 && strcmp(argv[1], "-r") == 0) {
        chemin_tasks = argv[2];
    } else {
        write(2, "Usage: ./erraid [-r BASE_PATH]\n", 32);
        return 1;
    }

    run_scheduler(chemin_tasks);
    return 0;
}