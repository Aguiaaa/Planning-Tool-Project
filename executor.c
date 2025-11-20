#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include "erraid.h"
#include "logger.h"
#include <stdlib.h>

bool validate_command(command *cmd, int *total_count) {
	if (cmd == NULL) {
		fprintf(stderr, "commande est NULL.\n");
		return false;
	}
	if (cmd->type != 0x5349 && cmd->type != 0x5351) {
		fprintf(stderr, "type invalide.\n");
		return false;
	}
	if (cmd->type == 0x5349) {
		if (cmd->args.ARGC == 0) {
			fprintf(stderr, "ARGC est 0.\n");
			return false;
		}
		if (cmd->args.ARGV == NULL) {
			fprintf(stderr, "ARGV est NULL.\n");
			return false;
		}
		for (uint32_t i = 0; i < cmd->args.ARGC; i++) {
			if (cmd->args.ARGV[i].DATA == NULL) {
				fprintf(stderr, "ARGV[%u].DATA est NULL.\n", i);
				return false;
			}
			if (cmd->args.ARGV[i].LENGTH == 0) {
				fprintf(stderr, "ARGV[%u].LENGTH est 0.\n", i);
				return false;
			}
		}
    } else {
        if (cmd->combinaison.ncmds == 0) {
            fprintf(stderr, "ncmds est 0.\n");
            return false;
        }
        *total_count += cmd->combinaison.ncmds;
        if (*total_count > 100) {
            fprintf(stderr, "les taches à plus de 100 commandes interdites.\n");
            return false;
        }
        if (cmd->combinaison.sous_command == NULL) {
            fprintf(stderr, "sous_command est NULL.\n");
            return false;
        }
        for (uint32_t i = 0; i < cmd->combinaison.ncmds; i++) {
            if (!validate_command(&cmd->combinaison.sous_command[i], total_count)) {
                return false;
            }
        }
    }
	return true;
}

bool validate_tasks(tasks *T) {
	if (T == NULL) {
		fprintf(stderr, "Le tableau tasks est malformé.\n");
		return false;
	}
	
	if (T->chemin.DATA == NULL) {
		fprintf(stderr, "chemin.DATA est NULL.\n");
		return false;
	}
	if (T->chemin.LENGTH == 0) {
		fprintf(stderr, "chemin.LENGTH est 0.\n");
		return false;
	}
	
	if (T->tm.MINUTES > 0x0FFFFFFFFFFFFFFFULL) {
		fprintf(stderr, "MINUTES invalide.\n");
		return false;
	}
	if (T->tm.HOURS > 0x00FFFFFF) {
		fprintf(stderr, "HOURS invalide.\n");
		return false;
	}
	if (T->tm.DAYSOFWEEK > 127) {
		fprintf(stderr, "DAYSOFWEEK invalide.\n");
		return false;
	}
	
	if (T->commandes == NULL) {
		fprintf(stderr, "commandes est NULL.\n");
		return false;
	}
	
	int total_count = 0;
    if (!validate_command(T->commandes, &total_count)) {
        return false;
    }
	
	return true;
}

bool should_run(tasks *T, struct tm *tm_now) {
	uint64_t bit_minute = 1ULL << tm_now->tm_min;
	uint32_t bit_hour = 1U << tm_now->tm_hour;
	uint8_t bit_day = 1 << tm_now->tm_wday;
	
	return (T->tm.MINUTES & bit_minute) &&
	       (T->tm.HOURS & bit_hour) &&
	       (T->tm.DAYSOFWEEK & bit_day);
}

// The exit status of a sequential list shall be the exit status of the last command in the list.
// Source: Definition of the Shell Command Language by https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html
// C'est pour cela que nous retournons le dernier code sortie et écrasons ceux des commandes intermédiaires dans le bloc SEQ 0x5351.
int execute_cmd(command *cmd, const char * chemin_stdout, const char * chemin_stderr) {
    if (cmd->type == 0x5349) {
    		pid_t pid = fork();

    		if (pid == 0) {
    			char **argv = malloc((cmd->args.ARGC + 1) * sizeof(char*));
    			if (argv == NULL) {
                    perror("malloc");
                    _exit(1);
    			}
    			for (uint32_t i = 0; i < cmd->args.ARGC; i++) {
    				argv[i] = (char*)cmd->args.ARGV[i].DATA;
    			}
    			argv[cmd->args.ARGC] = NULL;
                int fd_out = open((char*)chemin_stdout, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                int fd_err = open((char*)chemin_stderr, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                dup2(fd_out, STDOUT_FILENO);
                dup2(fd_err, STDERR_FILENO);
                close(fd_out);
                close(fd_err);
    			execvp(argv[0], argv);
    			perror("execvp");
    			free(argv);
    			_exit(1);
    		} else if (pid > 0) {
    			int status;
    			waitpid(pid, &status, 0);
                if (WIFEXITED(status)) {
                    return WEXITSTATUS(status);
                } else {
                    return -1;
                }
    		}
    		else { perror("fork");
    		    return -1;
    		}
    	} else if(cmd->type == 0x5351) {
            int dernier_code_sortie = 0;
            for (uint32_t i = 0; i < cmd->combinaison.ncmds; i++) {
                dernier_code_sortie = execute_cmd(&cmd->combinaison.sous_command[i], chemin_stdout, chemin_stderr);
            }
            return dernier_code_sortie;
        } fprintf(stderr, "Problème inattendu lors de l'exécution d'une commande.\n"); return -1;
}
void execute_task(tasks *T) {
    char chemin_stdout[512];
    char chemin_stderr[512];
    snprintf(chemin_stdout, sizeof(chemin_stdout), "%s/stdout", (char*)T->chemin.DATA);
    snprintf(chemin_stderr, sizeof(chemin_stderr), "%s/stderr", (char*)T->chemin.DATA);
    int codesortie = execute_cmd(T->commandes, chemin_stdout, chemin_stderr);
    log_execution((char*)T->chemin.DATA, codesortie);
}