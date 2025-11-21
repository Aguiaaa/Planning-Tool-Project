#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "erraid.h"
#include "lecteur.h"

int main(int argc, char *argv[]) {
    char *chemin_tasks;

    if (argc == 1) {
        char *user = getenv("USER");
        char chemin_aux[256];
        snprintf(chemin_aux, sizeof chemin_aux, "/tmp/%s/erraid/tasks", user);
        chemin_tasks = chemin_aux;
    } else if (argc == 3 && strcmp(argv[1], "-r") == 0) {
        chemin_tasks = argv[2];
    } else {
        write(2, "Usage: ./test_lecteur [-r BASE_PATH]\n", 38);
        return 1;
    }
    printf("Chemin tasks = %s\n", chemin_tasks);
    uint64_t nbtasks = number_of_tasks(chemin_tasks);
    printf("Nombre de tâches trouvées : %llu\n", (unsigned long long)nbtasks);
    tasks *T = read_tasks(chemin_tasks);
    if (!T) {
        write(2, "Erreur dans read_tasks\n", 24);
        return 1;
    }
    for (uint64_t i = 0; i < nbtasks; ++i) {
        printf("Task %llu:\n", (unsigned long long)T[i].ID);
        printf("  chemin = %.*s\n",
               T[i].chemin.LENGTH,
               T[i].chemin.DATA ? (char*)T[i].chemin.DATA : "(null)");
        printf("  MINUTES = 0x%016llx\n", (unsigned long long)T[i].tm.MINUTES);
        printf("  HOURS   = 0x%08x\n", T[i].tm.HOURS);
        printf("  DOW     = 0x%02x\n", T[i].tm.DAYSOFWEEK);
        printf("  commande = ");
        print_command(T[i].commandes);
        putchar('\n');
    }
    free_tasks(T, nbtasks);
    return 0;
}