#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include "erraid.h"
#include "lecteur.h"
#include "executor.h"

void run_scheduler(char *chemin_tasks) {
    uint64_t nbtasks = number_of_tasks(chemin_tasks);
    tasks *T = read_tasks(chemin_tasks);
    if (!T) {printf("Tasks est nul.\n");return;}
    while (1) {
        time_t now = time(NULL);
        struct tm *tm_now = localtime(&now);

        for (uint64_t i = 0; i < nbtasks; i++) {
            if (validate_tasks(&T[i]) && should_run(&T[i], tm_now)) {
                execute_task(&T[i]);
            }
        }
        printf("Tour de boucle.\n");
        sleep(60);
    }
    free_tasks(T, nbtasks);
}