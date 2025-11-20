#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "erraid.h"
#include <dirent.h>
#include "task_runner.h"


struct dirent *entry , *entry2 ; 
struct stat st;

/*
char chemin_erraid[256] , chemin_tasks[256] ,PATH1[256], PATH2[256] ; 

int run = 1 ; 

//initialiser les tubes de communications pour l'instant sans option -r
void init_pipes() {
    
    
    const char * user = getenv("USER") ; 
    snprintf (chemin_erraid, sizeof chemin_erraid , "/tmp/%s/erraid/", user) ; 
    if (mkdir(chemin_erraid, 0700) == -1 && errno != EEXIST) {
        perror("mkdir");
    }
    snprintf(chemin_tasks, sizeof chemin_tasks , "%stasks/" , chemin_erraid) ;
    if (mkdir(chemin_tasks, 0700) == -1 && errno != EEXIST) {
        perror("mkdir");
    }

    char chemin2[256] ; 
    snprintf(chemin2 ,sizeof chemin2, "%s/pipes/", chemin_erraid) ; 
    if (mkdir(chemin2, 0700) == -1 && errno != EEXIST) {
        perror("mkdir");
    }


    snprintf (PATH1, sizeof PATH1 , "%serraid-request-pipe", chemin2) ; 
    snprintf(PATH2 , sizeof PATH2, "%serraid-reply-pipe", chemin2) ; 
    if (mkfifo(PATH1, 0666) == -1 && errno != EEXIST) perror ("mkfifo request") ;
    if (mkfifo(PATH2, 0666) == -1 && errno != EEXIST) perror ("mkfifo reply") ; 

}

int init_pipes_r(char * c) {
    
    snprintf (chemin_erraid, sizeof chemin_erraid , "%s/erraid/", c) ; 
    if (mkdir(chemin_erraid, 0700) == -1 && errno != EEXIST) {
        perror("mkdir");
        return -1 ; 
    }
    char chemin2[256] ; 
    snprintf(chemin2 ,sizeof chemin2, "%spipes/", chemin_erraid) ; 
    if (mkdir(chemin2, 0700) == -1 && errno != EEXIST) {
        perror("mkdir");
        return -1 ; 
    }

    snprintf (PATH1, sizeof PATH1 , "%serraid-request-pipe", chemin2) ; 
    snprintf(PATH2 , sizeof PATH2, "%serraid-reply-pipe", chemin2) ; 
    if (mkfifo(PATH1, 0666) == -1 && errno != EEXIST) perror ("mkfifo request") ;
    if (mkfifo(PATH2, 0666) == -1 && errno != EEXIST) perror ("mkfifo reply") ; 

    return 1 ; 
}
*/

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
                if (S_ISDIR(st.st_mode)) { printf ("sous commande : %s\n", entry2->d_name) ; number_of_cmd++ ; }
                }
                
            }
            printf("nombres de sous comamndes : %d\n", number_of_cmd) ; 
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


int main (int argc, char *argv[]) {
    char * chemin_tasks ; 
    if (argc == 1) {
        char * user = getenv("USER") ;
        char chemin_aux[256] ;  
        snprintf (chemin_aux , sizeof chemin_aux, "/tmp/%s/erraid/tasks", user) ; 
        chemin_tasks = chemin_aux ; 
        printf ("%s\n", chemin_tasks) ; 
    } else if (argc == 3 && strcmp(argv[1], "-r") == 0) {
        chemin_tasks = argv[2] ; 
        printf ("%s\n", chemin_tasks) ; 
    } else {
        write(2, "Usage: ./erraid [-r BASE_PATH]\n", 32 );
        return 1;
    }


    printf("Chemin tasks = %s\n", chemin_tasks);

    uint64_t nbtasks = number_of_tasks(chemin_tasks);
    printf("Nombre de tâches trouvées : %llu\n",
           (unsigned long long)nbtasks);

    tasks *T = read_tasks(chemin_tasks);
    if (!T) {
        write(2, "Erreur dans read_tasks\n", 24);
        return 1;
    }
    while (1) {
        time_t t = time(NULL);
        struct tm now;
        localtime_r(&t, &now);

        if (now.tm_sec == 0)
            break;

        sleep(1);
    }

    while (1) {
        

        time_t t = time(NULL);
        struct tm now;
        localtime_r(&t, &now);
        uint64_t i ;
        for (i = 0; i < nbtasks; i++){
            if (should_run(&T[i], &now)) {
                printf("execution de la tache : %llu\n", (unsigned long long)T[i].ID);
                execute_task(&T[i]);
            }
        }

        sleep(60 - now.tm_sec) ; 
    }


    free(T);


    /*
    int fd_request = open(PATH1, O_RDONLY); //lire la requete du client 
    if (fd_request == -1) {exit(1) ; } 
    int fd_reply = open(PATH2, O_WRONLY) ;  // ecrire la reponse pour le client 
    if (fd_reply == -1) {exit(1) ; } 

    */
  
}


