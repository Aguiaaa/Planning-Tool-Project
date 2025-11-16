#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include "erraid.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

bool validate_command(command *cmd) {
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

bool should_run(tasks *T, struct tm *tm_now) {
	uint64_t bit_minute = 1ULL << tm_now->tm_min;
	uint32_t bit_hour = 1U << tm_now->tm_hour;
	uint8_t bit_day = 1 << tm_now->tm_wday;
	
	return (T->tm.MINUTES & bit_minute) &&
	       (T->tm.HOURS & bit_hour) &&
	       (T->tm.DAYSOFWEEK & bit_day);
}

void execute_task(tasks *T) {
	if (T->commandes->type == 0x5349) {
		pid_t pid = fork();
		
		if (pid == 0) {
			char **argv = malloc((T->commandes->args.ARGC + 1) * sizeof(char*));
			for (uint32_t i = 0; i < T->commandes->args.ARGC; i++) {
				argv[i] = (char*)T->commandes->args.ARGV[i].DATA;
			}
			argv[T->commandes->args.ARGC] = NULL;
			
			execvp(argv[0], argv);
			perror("execvp");
			_exit(1);
		} else if (pid > 0) {
			int status;
			waitpid(pid, &status, 0);
		}
	}
}