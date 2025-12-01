#define _XOPEN_SOURCE 700

#include "erraid.h"
#include "task_runner.h"





struct dirent *entry , *entry2 ; 
struct stat st;
extern char *optarg;
extern int optind;
char req [256] ; char rep [256] ; 

static void print_command(const command *cmd) {
    if (!cmd) {
        printf("(commande NULL)");
        return;
    }

    if (cmd->type == 0x5349) { 
        printf("[ARGC=%u] ", cmd->args.ARGC);
        for (uint32_t i = 0; i < cmd->args.ARGC; ++i) {
            string *s = &cmd->args.ARGV[i];
            printf("«%.*s»", s->LENGTH, (char *)s->DATA);
            if (i + 1 < cmd->args.ARGC)
                printf(" ");
        }
    } else {
        printf("(SEQ ");
        for (uint32_t i = 0; i < cmd->combinaison.ncmds; ++i) {
            print_command(&cmd->combinaison.sous_command[i]);
            if (i + 1 < cmd->combinaison.ncmds)
                printf("; ");
        }
        printf(")");
    }
}


uint64_t  number_of_tasks (char * chemin) {
    DIR *dir = opendir(chemin) ; 
    uint64_t cpt = 0 ; 
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name , "." ) && strcmp(entry->d_name , ".." ) ) {cpt++;}
    }
    closedir(dir) ; 

    return cpt ; 
}
void read_cmd (command * c , char * chemin, char * chemin_cmd) {
       
        char chemin_fichier_type [256];
        snprintf(chemin_fichier_type, sizeof chemin_fichier_type , "%s/type" , chemin_cmd) ;
        int fd_type = open (chemin_fichier_type , O_RDONLY); 
        uint16_t t ; 
        ssize_t n ; 
        n = read(fd_type , &t , 2 ) ; 
        if (n != 2) {close(fd_type) ; perror("erreur read type") ; return  ;}
        c->type =  be16toh(t) ; 
        close(fd_type) ; 

        if (c->type == 0x5349) {
            char chemin_argv [256] ; 
            snprintf(chemin_argv , sizeof chemin_argv ,"%s/argv" , chemin_cmd) ;
            int fd_argv = open (chemin_argv , O_RDONLY) ; 
            ssize_t n  ; 
            uint32_t argc ; 
            n = read (fd_argv , &argc , 4 ) ;  
            c->args.ARGC = be32toh(argc) ; 
            c->args.ARGV = malloc(c->args.ARGC * sizeof(string));
            for (uint32_t i = 0 ; i < c->args.ARGC ; ++i ) {
                uint32_t length ; 
                n = read (fd_argv , &length , 4) ;
                length = be32toh(length);
                c->args.ARGV[i].LENGTH = be32toh(length) ;
                c->args.ARGV[i].DATA = malloc (length + 1) ;  
                read(fd_argv, c->args.ARGV[i].DATA, length);
                c->args.ARGV[i].DATA[length] = '\0';
            }
            
            close(fd_argv) ; 
            return ; 

        }
        else {
            uint64_t number_of_cmd = 0 ; 
            DIR * d_cmd = opendir(chemin_cmd) ; 
            while ((entry2 = readdir(d_cmd)) != NULL )
            {
                if (strcmp(entry2->d_name , ".") != 0 && strcmp(entry2->d_name , "..") != 0) {  
                char path[256];
                snprintf(path, sizeof path, "%s/%s", chemin_cmd, entry2->d_name);
                stat(path, &st);
                if (S_ISDIR(st.st_mode)) {  number_of_cmd++ ; }
                }
                
            }
            closedir(d_cmd) ; 
            c->combinaison.ncmds = (uint32_t) number_of_cmd ; 
            c->combinaison.sous_command = malloc(number_of_cmd * sizeof(command));
            d_cmd = opendir(chemin_cmd) ; 

            while ((entry2 = readdir (d_cmd)) != NULL ) {
                if (strcmp(entry2->d_name, ".") != 0 && strcmp(entry2->d_name, "..") != 0) {

                    char path[256];
                    snprintf(path, sizeof path, "%s/%s", chemin_cmd, entry2->d_name);

                    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
                        uint32_t idx = strtoul(entry2->d_name, NULL, 10);

                        read_cmd(&c->combinaison.sous_command[idx], path, path);
                    }
                }
            }
            closedir(d_cmd) ;

        

    }
}
tasks *  read_tasks (char * chemin) {
    tasks * TASKS = malloc (sizeof(tasks) * number_of_tasks(chemin)) ; 
    DIR *dir = opendir(chemin) ; 
    uint64_t  i = 0 ; 
    while ((entry = readdir(dir)) != NULL) {
        tasks tache ; 
        if (strcmp(entry->d_name , ".") != 0 && strcmp(entry->d_name , "..") != 0) {
            tache.ID = strtoull(entry->d_name, NULL, 10) ; 
            char chemin_tache[256]; 
            snprintf(chemin_tache, sizeof chemin_tache , "%s/%s" ,chemin , entry->d_name ) ; 
            
            uint32_t length = strlen(chemin_tache);
            tache.chemin.DATA = malloc(length + 1);
            strcpy((char*)tache.chemin.DATA, chemin_tache);
            tache.chemin.LENGTH = length;
            printf("chemin tache : %s\n ", tache.chemin.DATA) ; 
           
            
            char chemin_fichier_timing[256] ;
            snprintf (chemin_fichier_timing , sizeof chemin_fichier_timing , "%s/timing" , chemin_tache ) ;  
            int fd = open(chemin_fichier_timing , O_RDONLY) ; 
            if (fd == -1) {perror("erreur d'ouverture de timing" ) ; return NULL ; }
            uint64_t minutes ; uint32_t hours ; uint8_t daysofweek ;  
            ssize_t n ; 
            n = read (fd , &minutes , 8) ; 
            if (n != 8) {close(fd) ; perror("erreur read timing") ; return NULL;  } 
            n = read (fd , &hours , 4) ;
            if (n != 4) {close(fd) ; perror("erreur read timing") ; return NULL ;  } 
            n = read (fd , &daysofweek, 1) ; 
            if (n != 1) {close(fd) ; perror("erreur read timing") ; return NULL; } 
            close(fd) ; 

            tache.tm.MINUTES =  be64toh(minutes);
            tache.tm.HOURS = be32toh(hours);
            tache.tm.DAYSOFWEEK = daysofweek ; 
            tache.commandes = malloc(sizeof(command));
            char chemin_cmd [256] ; 
            snprintf(chemin_cmd , sizeof chemin_cmd , "%s/cmd", chemin_tache) ; 
            read_cmd(tache.commandes, chemin_tache, chemin_cmd) ; 
            TASKS[i] = tache ;
            i++ ;  
        }
    }
    return TASKS; 


}

void write_u16(int fd, uint16_t v) {
    uint16_t be = htobe16(v);
    write(fd, &be, 2);
}
void write_u32(int fd, uint32_t v) {
    uint32_t be = htobe32(v);
    write(fd, &be, 4);
}
void write_u64(int fd, uint64_t v) {
    uint64_t be = htobe64(v);
    write(fd, &be, 8);
}

void write_cmd_recursive(int fd, command *cmd) {
    write_u16(fd, cmd->type);
    if (cmd->type == 0x5349) {
        write_u32(fd, cmd->args.ARGC);
        for (uint32_t i = 0; i < cmd->args.ARGC; i++) {
            write_u32(fd, cmd->args.ARGV[i].LENGTH);
            write(fd, cmd->args.ARGV[i].DATA, cmd->args.ARGV[i].LENGTH);
        }
    } else if (cmd->type == 0x5351) { // SQ
        write_u32(fd, cmd->combinaison.ncmds);
        for (uint32_t i = 0; i < cmd->combinaison.ncmds; i++) {
            write_cmd_recursive(fd, &cmd->combinaison.sous_command[i]);
        }
    }
}

void handle_ls(char *rep_path, tasks *T, uint64_t nbtasks) {
    int fd_rep = open(rep_path, O_WRONLY);
    if (fd_rep == -1) return;

    write_u16(fd_rep, 0x4F4B); // OK
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

int main (int argc, char *argv[]) {
    char chemin_tasks [256] , chemin_pipes[256]; bool pipes_dir_defined = false ;
    char * user = getenv("USER") ;  
    snprintf (chemin_tasks , sizeof chemin_tasks, "/tmp/%s/erraid/tasks", user) ; 
    snprintf(chemin_pipes, sizeof chemin_pipes, "/tmp/%s/erraid/pipes", user);
    /*if (argc == 1) {
        char * user = getenv("USER") ;  
        snprintf (chemin_tasks , sizeof chemin_tasks, "/tmp/%s/erraid/tasks", user) ; 
        snprintf(chemin_pipes, sizeof chemin_pipes, "/tmp/%s/erraid/pipes", user);  

    } else if (argc == 3 && strcmp(argv[1], "-r") == 0) {
        snprintf (chemin_tasks , sizeof chemin_tasks, "%s/tasks", argv[2]) ;
        snprintf(chemin_pipes, sizeof chemin_pipes, "%s/pipes", argv[2]);  
    }
       else if (argc == 3 && strcmp(argv[1], "-p") == 0) {
            snprintf(chemin_pipes, sizeof chemin_pipes, "%s/pipes", argv[2]);  
    
    } else {
        write(2, "Usage: ./erraid [-r BASE_PATH]\n", 32 );
        return 1;
    }*/
    int opt;  
    while ((opt = getopt(argc, argv, "r:p:")) != -1) {
        switch (opt) {
            case 'r':
                snprintf (chemin_tasks , sizeof chemin_tasks, "%s/tasks", optarg) ;  
                break;
            case 'p':
                snprintf(chemin_pipes, sizeof chemin_pipes, "%s/pipes", optarg);  
                snprintf(req , sizeof req , "%s/erraid-request-pipe", chemin_pipes) ; 
                snprintf(rep , sizeof rep , "%s/erraid-reply-pipe", chemin_pipes) ;
                
                pipes_dir_defined = true ; 
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
                pipes_dir_defined = true ; 
                exit(EXIT_FAILURE);
            } 
        }
        if (mkfifo (rep , 0622) == -1) {
            if (errno != EEXIST) { 
                perror("mkfifo reponse");
                pipes_dir_defined = true ; 
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

    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGPIPE, &sa, NULL);

    int fd_req = open(req, O_RDWR); 
    if (fd_req == -1) { perror("open req pipe"); return 1; }

    printf("Démon démarré. En attente...\n");

    while (1) {
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        
        struct timeval tv;
        tv.tv_sec = 60 - tm_now.tm_sec; 
        tv.tv_usec = 0;

        fd_set rdfs;
        FD_ZERO(&rdfs);
        FD_SET(fd_req, &rdfs);

        int ret = select(fd_req + 1, &rdfs, NULL, NULL, &tv);

        if (ret == 0) {
            now = time(NULL);
            localtime_r(&now, &tm_now);
            
            for (uint64_t i = 0; i < nbtasks; i++) {
                if (should_run(&T[i], &tm_now)) {
                    printf("Lancement tache ID %llu\n", (unsigned long long)T[i].ID);
                    if (fork() == 0) {
                        if (fork() == 0) { 
                            execute_task(&T[i]);
                            exit(0);
                        }
                        exit(0); 
                    }
                    wait(NULL); 
                }
            }
        }
        
        else if (ret > 0 && FD_ISSET(fd_req, &rdfs)) {
            uint16_t opcode_be;

            if (read(fd_req, &opcode_be, 2) == 2) {
                uint16_t opcode = be16toh(opcode_be);
                
                if (opcode == 0x4C53) { 
                    printf("Reçu commande LIST\n");
                    handle_ls(rep, T, nbtasks);
                }
            }
        }
    }

    close(fd_req);

    free(T);

  
}


