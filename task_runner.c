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



/* bool validate_command(command *cmd) {
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
		if (cmd->combinaison.ncmds == 0) { //TODO : Rejeter les structures contenant un nombre trop grand de commandes avec un nombre limite d'appels récursifs et un compteur de ncmds.
			fprintf(stderr, "ncmds est 0.\n");
			return false;
		}
		if (cmd->combinaison.sous_command == NULL) {
			fprintf(stderr, "sous_command est NULL.\n");
			return false;
		}
		for (uint32_t i = 0; i < cmd->combinaison.ncmds; i++) {
			if (!validate_command(&cmd->combinaison.sous_command[i])) {
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
	
	if (!validate_command(T->commandes)) {
		return false;
	}
	
	return true;
}
*/
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

void run_simple(tasks *T, command *cmd) {

    pid_t pid = fork();

    if (pid == 0) {
        char path_out[256], path_err[256];
        snprintf(path_out, sizeof path_out, "%.*s/stdout",
                 T->chemin.LENGTH, T->chemin.DATA);
        snprintf(path_err, sizeof path_err, "%.*s/stderr",
                 T->chemin.LENGTH, T->chemin.DATA);

        int fdout = open(path_out, O_WRONLY|O_CREAT|O_TRUNC, 0600);
        int fderr = open(path_err, O_WRONLY|O_CREAT|O_TRUNC, 0600);

        dup2(fdout, STDOUT_FILENO);
        dup2(fderr, STDERR_FILENO);

        char **argv = malloc((cmd->args.ARGC + 1) * sizeof(char*));
        for (uint32_t i = 0; i < cmd->args.ARGC; i++)
            argv[i] = (char *)cmd->args.ARGV[i].DATA;
        argv[cmd->args.ARGC] = NULL;

        execvp(argv[0], argv);
        _exit(1);
    }

    else {
        int st;
        waitpid(pid, &st, 0);
    }
}



int run_sequence(tasks *T, command *sub) {

    pid_t pid = fork();

    if (pid == 0) {
        char path_out[256], path_err[256];
        snprintf(path_out, sizeof path_out, "%.*s/stdout",
                 T->chemin.LENGTH, T->chemin.DATA);
        snprintf(path_err, sizeof path_err, "%.*s/stderr",
                 T->chemin.LENGTH, T->chemin.DATA);

        int fdout = open(path_out, O_WRONLY|O_CREAT|O_APPEND, 0600);
        int fderr = open(path_err, O_WRONLY|O_CREAT|O_APPEND, 0600);

        dup2(fdout, STDOUT_FILENO);
        dup2(fderr, STDERR_FILENO);

        char **argv = malloc((sub->args.ARGC + 1) * sizeof(char*));
        for (uint32_t k = 0; k < sub->args.ARGC; k++)
            argv[k] = (char *)sub->args.ARGV[k].DATA;
        argv[sub->args.ARGC] = NULL;

        execvp(argv[0], argv);
        _exit(1);
    }
    else {
        int st;
        waitpid(pid, &st, 0);
        return st;
    }
}



void execute_task(tasks *T) {

    command *cmd = T->commandes;
    char path_out[256], path_err[256];
    snprintf(path_out, sizeof path_out, "%.*s/stdout",
             T->chemin.LENGTH, T->chemin.DATA);
    snprintf(path_err, sizeof path_err, "%.*s/stderr",
             T->chemin.LENGTH, T->chemin.DATA);

    int fdout0 = open(path_out, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fdout0 < 0) perror("open stdout initial");
    close(fdout0);

    int fderr0 = open(path_err, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fderr0 < 0) perror("open stderr initial");
    close(fderr0);

    if (cmd->type == 0x5349) {
        run_simple(T, cmd);
        return;
    }
    if (cmd->type == 0x5351) {
        for (uint32_t sc_i = 0; sc_i < cmd->combinaison.ncmds; sc_i++) {
            command *sub = &cmd->combinaison.sous_command[sc_i];
            int st = run_sequence(T, sub);
            if ((st & 0xFF00) != 0)
                break;
        }
    }
}




