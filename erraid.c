#define _XOPEN_SOURCE 700

#include "erraid.h"
#include "task_runner.h"
#include "parsing_tasks.h"
#include "protocole.h"

extern char *optarg;
extern int optind;
char req [256] ; char rep [256] ; 

void write_cmd_recursive(int fd, command *cmd) {
    write_u16(fd, cmd->type);
    if (cmd->type == TYPE_SIMPLE) {
        write_u32(fd, cmd->args.ARGC);
        for (uint32_t i = 0; i < cmd->args.ARGC; i++) {
            write_u32(fd, cmd->args.ARGV[i].LENGTH);
            write(fd, cmd->args.ARGV[i].DATA, cmd->args.ARGV[i].LENGTH);
        }
    } else if (cmd->type == TYPE_SEQ) { 
        write_u32(fd, cmd->combinaison.ncmds);
        for (uint32_t i = 0; i < cmd->combinaison.ncmds; i++) {
            write_cmd_recursive(fd, &cmd->combinaison.sous_command[i]);
        }
    }
}

void traiter_xoe(char *rep_path, char *base_path, uint64_t id, char *filename) {
    int fd_rep = open(rep_path, O_WRONLY);
    if (fd_rep == -1) return;

    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/%llu/%s", base_path, (unsigned long long)id, filename);

    int fd_file = open(file_path, O_RDONLY);
    if (fd_file == -1) {
        write_u16(fd_rep, REP_ERR); 
    } else {
        write_u16(fd_rep, REP_OK); 
        struct stat st;
        fstat(fd_file, &st);
        if (strcmp(filename, "times-exitcodes") == 0) {
             write_u32(fd_rep, (uint32_t)(st.st_size / 10));
        } else {
             write_u32(fd_rep, (uint32_t)st.st_size);
        }
        char buf[1024];
        ssize_t n;
        while ((n = read(fd_file, buf, sizeof(buf))) > 0) {
            write(fd_rep, buf, n);
        }
        close(fd_file);
    }
    close(fd_rep);
}

void lister_taches(char *rep_path, tasks *T, uint64_t nbtasks) {
    int fd_rep = open(rep_path, O_WRONLY);
    if (fd_rep == -1) return;

    write_u16(fd_rep, 0x4F4B); 
    write_u32(fd_rep, (uint32_t)nbtasks);

    for (uint64_t i = 0; i < nbtasks; i++) {
        write_u64(fd_rep, T[i].ID);
        write_u64(fd_rep, T[i].tm.MINUTES);
        write_u32(fd_rep, T[i].tm.HOURS);
        write(fd_rep, &T[i].tm.DAYSOFWEEK, 1);
        write_cmd_recursive(fd_rep, T[i].commandes);
    }
    close(fd_rep);
}
void gestion_alarme(int sig) {
    (void)sig; 
}

int main (int argc, char *argv[]) {
    char chemin_tasks [256] , chemin_pipes[256]; 
    char * user = getenv("USER") ;  
    snprintf (chemin_tasks , sizeof chemin_tasks, "/tmp/%s/erraid/tasks", user) ; 
    snprintf(chemin_pipes, sizeof chemin_pipes, "/tmp/%s/erraid/pipes", user);

    int opt;  
    while ((opt = getopt(argc, argv, "r:p:")) != -1) {
        switch (opt) {
            case 'r':
                snprintf (chemin_tasks , sizeof chemin_tasks, "%s/tasks", optarg) ;  
                break;
            case 'p':
                snprintf(chemin_pipes, sizeof(chemin_pipes), "%s", optarg); 
                snprintf(req , sizeof req , "%s/erraid-request-pipe", chemin_pipes) ; 
                snprintf(rep , sizeof rep , "%s/erraid-reply-pipe", chemin_pipes) ;
                break;
            case '?': 
                fprintf(stderr, "Usage: %s [-r RUN_DIRECTORY] [-p PIPES_DIR]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    printf("Chemin tasks = %s\n", chemin_tasks);
    printf("Chemin pipes = %s\n", chemin_pipes);

if (mkfifo (req , 0622) == -1) {
            if (errno != EEXIST) { 
                perror("mkfifo requete");
                exit(EXIT_FAILURE);
            } 
        }
        if (mkfifo (rep , 0622) == -1) {
            if (errno != EEXIST) { 
                perror("mkfifo reponse");
                exit(EXIT_FAILURE);
            } 
        }
    uint64_t nbtasks = number_of_tasks(chemin_tasks);
    printf("Nombre de tâches trouvées : %llu\n",
           (unsigned long long)nbtasks);

    tasks *T = read_tasks(chemin_tasks);
    if (!T) {
        write(2, "Erreur dans read_tasks\n", 24);
        return 1;
    }

    struct sigaction sa_ign;
    sa_ign.sa_handler = SIG_IGN;
    sigemptyset(&sa_ign.sa_mask);
    sa_ign.sa_flags = 0;
    sigaction(SIGPIPE, &sa_ign, NULL);

    struct sigaction sa_alarm;
    sa_alarm.sa_handler = gestion_alarme;
    sigemptyset(&sa_alarm.sa_mask);

    sa_alarm.sa_flags = 0; 
    sigaction(SIGALRM, &sa_alarm, NULL);


    int fd_req = open(req, O_RDWR); 
    if (fd_req == -1) { perror("open req"); return 1; }

    printf("Démon prêt.\n");

    while (1) {
        
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        int sleep = 60 - tm_now.tm_sec;
        
        alarm(sleep); 

        uint16_t opcode_be;
        ssize_t n = read(fd_req, &opcode_be, 2);
        uint16_t opcode = be16toh(opcode_be);

        alarm(0);

        if (n == -1 && errno == EINTR) {
            printf("[Timer] Nouvelle minute\n");
            now = time(NULL);
            localtime_r(&now, &tm_now);
            for (uint64_t i = 0; i < nbtasks; i++) {
                if (should_run(&T[i], &tm_now)) {
                    if (fork() == 0) {
                        close(fd_req);
                        if (fork() == 0) { execute_task(&T[i]); exit(0); }
                        exit(0);
                    }
                    wait(NULL);
                }
            }
        }
        
        else if (n == 2 ) {

            if ( opcode == LIST) { // l 
                printf("[Client] Reçu LS\n");
                lister_taches(rep, T, nbtasks);
            }
            else if (opcode == TIMES_EXITCODES) { // x
                uint64_t id_be, id;
                read(fd_req, &id_be, 8); 
                id = be64toh(id_be);
                
                printf("[Client] Historique demandé pour ID %llu\n", (unsigned long long)id);
                traiter_xoe(rep, chemin_tasks, id, "times-exitcodes");
            }

            else if (opcode == STDOUT) { // o 
                uint64_t id_be, id;
                read(fd_req, &id_be, 8);
                id = be64toh(id_be);

                printf("[Client] Stdout demandé pour ID %llu\n", (unsigned long long)id);
                traiter_xoe(rep, chemin_tasks, id, "stdout");
            }

            else if (opcode == STDERR) { // e
                uint64_t id_be, id;
                read(fd_req, &id_be, 8);
                id = be64toh(id_be);

                printf("[Client] Stderr demandé pour ID %llu\n", (unsigned long long)id);
                traiter_xoe(rep, chemin_tasks, id, "stderr");
            }
        }
        
        }

    close(fd_req);
    unlink(req); 
    unlink(rep);
    free(T);

  
}


