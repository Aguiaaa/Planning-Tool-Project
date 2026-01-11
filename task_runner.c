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
#include "protocole.h"

bool should_run(tasks *T, struct tm *tm_now) {
    return (T->tm.MINUTES & (1ULL << tm_now->tm_min)) &&
           (T->tm.HOURS & (1U << tm_now->tm_hour)) &&
           (T->tm.DAYSOFWEEK & (1 << tm_now->tm_wday));
}

void log_execution(tasks *T, int exit_code) {
    char path[512];
    snprintf(path, sizeof(path), "%.*s/times-exitcodes",
             T->chemin.LENGTH, (char *)T->chemin.DATA);

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("open log");
        return;
    }

    write_64(fd, (uint64_t)time(NULL));
    write_16(fd, (uint16_t)exit_code);
    
    close(fd);
}


int run(tasks *T, command *cmd) {
    if (cmd->type == TYPE_SIMPLE) {
        pid_t pid = fork();
        if (pid == 0) {
            char *args[cmd->args.ARGC + 1];
            for (uint32_t i = 0; i < cmd->args.ARGC; i++) {
                args[i] = (char *)cmd->args.ARGV[i].DATA;
            }
            args[cmd->args.ARGC] = NULL;

            execvp(args[0], args);
            perror("execvp");
            exit(127);
        }
        int status;
        while (waitpid(pid, &status, 0) == -1) {
            if (errno != EINTR) return -1;
        }
        if (WIFEXITED(status)) return WEXITSTATUS(status);
        return -1;
    } else if (cmd->type == TYPE_SEQ) {
        int ret = 0;
        for (uint32_t i = 0; i < cmd->combinaison.ncmds; i++) {
            ret = run(T, &cmd->combinaison.sous_command[i]);
        }
        return ret;
    } else if (cmd->type == TYPE_PL) {
        int n = cmd->combinaison.ncmds;
        int prev_pipe = -1;
        int pipefd[2];

        for (int i = 0; i < n; i++) {
            if (i < n - 1) {
                if (pipe(pipefd) == -1) { perror("pipe"); return -1; }
            }

            pid_t pid = fork();
            if (pid == 0) {
                if (prev_pipe != -1) {
                    dup2(prev_pipe, STDIN_FILENO);
                    close(prev_pipe);
                }
                if (i < n - 1) {
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                    close(pipefd[0]);
                }
                exit(run(T, &cmd->combinaison.sous_command[i]));
            }

            if (prev_pipe != -1) close(prev_pipe);
            if (i < n - 1) {
                prev_pipe = pipefd[0];
                close(pipefd[1]);
            }
        }
        while (wait(NULL) > 0);
        return 0;
    } else if (cmd->type == TYPE_IF) {
        if (cmd->combinaison.ncmds < 2) return -1;
        int ret_cond = run(T, &cmd->combinaison.sous_command[0]);

        if (ret_cond == 0) return run(T, &cmd->combinaison.sous_command[1]);
        else if (cmd->combinaison.ncmds >= 3) return run(T, &cmd->combinaison.sous_command[2]);
        
        return 0;
    }
    return -1;
}

void execute_task(tasks *T) {
    char out[512], err[512];
    snprintf(out, 512, "%.*s/stdout", T->chemin.LENGTH, (char *)T->chemin.DATA);
    snprintf(err, 512, "%.*s/stderr", T->chemin.LENGTH, (char *)T->chemin.DATA);

    int fo = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int fe = open(err, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fo == -1 || fe == -1) {
        if (fo != -1) close(fo);
        if (fe != -1) close(fe);
        return;
    }
    
    dup2(fo, STDOUT_FILENO); 
    dup2(fe, STDERR_FILENO); 
    close(fo);
    close(fe);

    int status = run(T, T->commandes);
    
    log_execution(T, status);
}