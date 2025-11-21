#include <stdio.h>
#include <time.h>
#include <string.h>
#include "erraid.h"
#include <stdint.h>

void log_execution(const char *chemin_task, int codesortie) {
    char chemin[512];
    snprintf(chemin, sizeof(chemin), "%s/times-exitcodes", chemin_task);
    FILE *f = fopen(chemin, "a");
    if (f == NULL) {
        perror("fopen");
        return;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y/%m/%d\t%H:%M:%S", tm_info);
    fprintf(f, "%s\t%d\n", timestamp, codesortie);

    fclose(f);
}