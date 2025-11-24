#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include "erraid.h"

bool should_run(tasks *T, struct tm *tm_now) {
	return (T->tm.MINUTES & (1ULL << tm_now->tm_min)) &&
           (T->tm.HOURS & (1U << tm_now->tm_hour)) &&
           (T->tm.DAYSOFWEEK & (1 << tm_now->tm_wday));
}
void log(tasks *T, int exit_code) {
    char path[512];
    snprintf(path, sizeof(path), "%.*s/times-exitcodes", 
             T->chemin.LENGTH, (char *)T->chemin.DATA);

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("open log");
        return;
    }

    uint64_t now = htobe64((uint64_t)time(NULL));
    uint16_t code = htobe16((uint16_t)exit_code);

    write(fd, &now, sizeof(now));
    write(fd, &code, sizeof(code));
    close(fd);
}

int run(tasks *T, command *cmd) {
    if (cmd->type == 0x5349) { 
        pid_t pid = fork();
        if (pid == 0) {

            char out[512], err[512];
            snprintf(out, 512, "%.*s/stdout", T->chemin.LENGTH, (char *)T->chemin.DATA);
            snprintf(err, 512, "%.*s/stderr", T->chemin.LENGTH, (char *)T->chemin.DATA);
            int fo = open(out, O_WRONLY | O_CREAT | O_APPEND, 0644);
            int fe = open(err, O_WRONLY | O_CREAT | O_APPEND, 0644);
            dup2(fo, 1);
            dup2(fe, 2); 
            char *args[cmd->args.ARGC + 1];
            for (uint32_t i = 0; i < cmd->args.ARGC; i++) {
                args[i] = (char *)cmd->args.ARGV[i].DATA;
            }
            args[cmd->args.ARGC] = NULL;

            execvp(args[0], args);
            exit(1); 
        } 
        
        int status;
        waitpid(pid, &status, 0);
        return WEXITSTATUS(status);
    } 
    else { 
        int ret = 0;
        for (uint32_t i = 0; i < cmd->combinaison.ncmds; i++) {
            ret = run(T, &cmd->combinaison.sous_command[i]);
        }
        return ret;
    }
}

void execute_task(tasks *T) {
    char out[512], err[512];
    snprintf(out, 512, "%.*s/stdout", T->chemin.LENGTH, (char *)T->chemin.DATA);
    snprintf(err, 512, "%.*s/stderr", T->chemin.LENGTH, (char *)T->chemin.DATA);
    int fo = open(out, O_RDONLY| O_CREAT | O_TRUNC, 0644);
    int fe = open(err, O_RDONLY | O_CREAT | O_TRUNC, 0644);
    close(fo) ; 
    close(fe) ; 
    int status = run(T, T->commandes);
    log(T, status);
}


