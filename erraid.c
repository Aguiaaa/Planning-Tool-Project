#define _XOPEN_SOURCE 700

#include "erraid.h"
#include "task_runner.h"
#include "parsing_tasks.h"
#include "protocole.h"
#include <poll.h> 


extern char *optarg;
extern int optind;
char req [256] ; char rep [256] ; 


volatile sig_atomic_t running = 1; 

void handler_arret(int sig) {
    (void)sig; 
    running = 0; 
}

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
    int fd_rep = open(rep_path, O_RDWR);
    if (fd_rep == -1) return;

    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "%s/%llu",
             base_path, (unsigned long long)id);

    struct stat st_task;
    if (stat(dir_path, &st_task) == -1) {
        int err = errno;
        write_u16(fd_rep, 0x4552);       

        if (err == ENOENT) {
            write_u16(fd_rep, 0x4E46);   
        } else {
            write_u16(fd_rep, 0x4E52);  
        }
        close(fd_rep);
        return;
    }

    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/%llu/%s",
             base_path, (unsigned long long)id, filename);

    int fd_file = open(file_path, O_RDONLY);
    if (fd_file == -1) {
        write_u16(fd_rep, 0x4552);        
        write_u16(fd_rep, 0x4E52);       
        close(fd_rep);
        return;
    }
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
    close(fd_rep);
}


void lister_taches(char *rep_path, tasks *T, uint64_t nbtasks) {
    int fd_rep = open(rep_path, O_RDWR);
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
void handler_alarme(int sig) {
    (void)sig; 
}

int main (int argc, char *argv[]) {
    char chemin_tasks [256] , chemin_pipes[256]; 
    char * user = getenv("USER") ;

    char *arg_r = NULL;
    char *arg_p = NULL;
    int opt;  
    while ((opt = getopt(argc, argv, "r:p:")) != -1) {
        switch (opt) {
            case 'r': arg_r = optarg; break;
            case 'p': arg_p = optarg; break;
            case '?': 
                fprintf(stderr, "Usage: %s [-r RUN_DIRECTORY] [-p PIPES_DIR]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (arg_r) snprintf(chemin_tasks, sizeof(chemin_tasks), "%s/tasks", arg_r);
    else if (user) snprintf(chemin_tasks, sizeof(chemin_tasks), "/tmp/%s/erraid/tasks", user);
    else strcpy(chemin_tasks, "/tmp/erraid/tasks");

    if (arg_p) snprintf(chemin_pipes, sizeof(chemin_pipes), "%s", arg_p);
    else if (arg_r) snprintf(chemin_pipes, sizeof(chemin_pipes), "%s/pipes", arg_r);
    else if (user) snprintf(chemin_pipes, sizeof(chemin_pipes), "/tmp/%s/erraid/pipes", user);
    else strcpy(chemin_pipes, "/tmp/erraid/pipes");

    printf("Chemin tasks = %s\n", chemin_tasks);
    printf("Chemin pipes = %s\n", chemin_pipes);

    snprintf(req , sizeof req , "%s/erraid-request-pipe", chemin_pipes) ; 
    snprintf(rep , sizeof rep , "%s/erraid-reply-pipe", chemin_pipes) ;

   if (!arg_p && !arg_r && user) {
        char parent[256];
        snprintf(parent, sizeof(parent), "/tmp/%s/erraid", user);
        mkdir(parent, 0700); 
    }
    struct stat st;
    if (stat(chemin_pipes, &st) == -1) mkdir(chemin_pipes, 0700);

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
        
    struct sigaction sa_ign;
    sa_ign.sa_handler = SIG_IGN;
    sigemptyset(&sa_ign.sa_mask);
    sa_ign.sa_flags = 0;
    sigaction(SIGPIPE, &sa_ign, NULL);


    struct sigaction sa_arret;
    sa_arret.sa_handler = handler_arret;
    sigemptyset(&sa_arret.sa_mask);
    sa_arret.sa_flags = 0; 
    sigaction(SIGINT, &sa_arret, NULL); 
    sigaction(SIGTERM, &sa_arret, NULL);
    
    int fd_req = open(req, O_RDWR); 
    if (fd_req == -1) { perror("open req"); unlink(req); unlink(rep); return 1; }

    uint64_t nbtasks = number_of_tasks(chemin_tasks);
    printf("Nombre de tâches trouvées : %llu\n",
           (unsigned long long)nbtasks);

    tasks *T = read_tasks(chemin_tasks);

    if (!T && nbtasks > 0) {
        write(2, "Erreur lecture tasks\n", 21);
        close(fd_req); unlink(req); unlink(rep);
        return 1;
    }

    printf("Démon prêt.\n");
    fflush(stdout);

    struct pollfd fds[1] ; 
    fds[0].fd = fd_req ; 
    fds[0].events = POLLIN ; 

    time_t last_check = time(NULL) ; 
    struct tm tm_last ; 
    localtime_r(&last_check , &tm_last) ;
    int last_minute = tm_last.tm_min ;

    while (running) {
        
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        
        int timeout_ms ; 
        if  (tm_now.tm_min != last_minute) {
            timeout_ms = 0 ;
        } else {
            int seconds_left = 60 - tm_now.tm_sec ; 
            timeout_ms = seconds_left * 1000;
        }

        int ret = poll(fds , 1 , timeout_ms) ; 
        if (ret == -1) {
            if (errno == EINTR) continue ; 
            perror("poll") ; 
            break ; 
        }

        now = time(NULL);
        localtime_r(&now, &tm_now);
        if (tm_now.tm_min != last_minute) {
            printf("[Timer] Nouvelle minute\n");
            for (uint64_t i = 0; i < nbtasks; i++) {
                if (should_run(&T[i], &tm_now)) {
                    pid_t pid = fork() ; 
                    if (pid == -1) {
                        perror("fork") ;
                        continue ; 
                    }
                    if (pid == 0) {
                        close(fd_req);
                        if (fork() == 0) { execute_task(&T[i]); exit(0); }
                        exit(0);
                    }
                    int status ;
                    if (waitpid(pid, &status , 0) == -1) {
                        perror("waitpid") ;
                    }
                }
            }
            last_minute = tm_now.tm_min ; 
        }
        
        if (ret > 0 && (fds[0].revents & POLLIN) ) {
            uint16_t opcode_be;
            ssize_t n = read(fd_req, &opcode_be, 2);

            if (n != 2) continue ; 
            uint16_t opcode = be16toh(opcode_be);
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


