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
uint64_t  number_of_tasks (char * chemin) {
    DIR *dir = opendir(chemin) ; 
    uint64_t cpt = 0 ; 
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name , "." ) && strcmp(entry->d_name , ".." ) ) {cpt++;}
    }
    closedir(dir) ; 

    return cpt ; 
}
void read_cmd (command * c , char * chemin) {
        char chemin_cmd [256] ; 
        snprintf(chemin_cmd , sizeof chemin_cmd , "%s/cmd", chemin) ; 
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
            for (uint32_t i = 0 ; i < c->args.ARGC ; i++ ) {
                uint32_t length ; 
                n = read (fd_argv , &length , 4) ;
                c->args.ARGV[i].LENGTH = be32toh(length) ;
                c->args.ARGV[i].DATA = malloc (length + 1) ;  
                read(fd_argv, c->args.ARGV[i].DATA, length);
                c->args.ARGV[i].DATA[length] = '\0';
            }
            close(fd_argv) ; 

        }
        else {
            uint64_t number_of_cmd = 0 ; 
            DIR * d_cmd = opendir(chemin_cmd) ; 
            while ((entry2 = readdir(d_cmd)) != NULL )
            {
                char path[256];
                snprintf(path, sizeof path, "%s/%s", chemin_cmd, entry2->d_name);
                struct stat st;
                stat(path, &st);
                if (S_ISDIR(st.st_mode)) { number_of_cmd++ ; }
            }
            closedir(d_cmd) ; 
            c->combinaison.ncmds = number_of_cmd ; 
            c->combinaison.sous_command = malloc(number_of_cmd * sizeof(command));
            d_cmd = opendir(chemin_cmd) ; 
            uint32_t i = strtoul(entry->d_name, NULL, 10); 
            while ((entry2 = readdir (d_cmd)) != NULL) {
                char chemin_sous_cmd [256] ; 
                snprintf(chemin_sous_cmd, sizeof chemin_sous_cmd, "%s/%s", chemin_cmd, entry2->d_name) ; 
                read_cmd(&(c->combinaison.sous_command[i]) , chemin_sous_cmd) ; 
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
            
            uint32_t length = (uint32_t) (strlen(chemin) + strlen(entry->d_name) + 1) ; 
            tache.chemin.DATA = malloc(length + 1);
            memcpy(tache.chemin.DATA, chemin_tache, length + 1);
            printf("chemin tache : %s/n ", tache.chemin.DATA) ; 
            tache.chemin.LENGTH = length ;  
            
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
            read_cmd(tache.commandes, chemin_tache) ; 
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

    //TEST pour connaitre le chemin du dossier de stockage /tasks 
    printf("Chemin tasks = %s\n", chemin_tasks);

    //debut de TEST pour read_tasks()
    uint64_t nbtasks = number_of_tasks(chemin_tasks);
    printf("Nombre de tâches trouvées : %llu\n",
           (unsigned long long)nbtasks);

    tasks *T = read_tasks(chemin_tasks);
    if (!T) {
        write(2, "Erreur dans read_tasks\n", 24);
        return 1;
    }
    for (uint64_t i = 0; i < nbtasks; ++i) {
        printf("Task %llu:\n", (unsigned long long)T[i].ID);
        printf("  chemin = %.*s\n",
               T[i].chemin.LENGTH,
               T[i].chemin.DATA ? (char*)T[i].chemin.DATA : "(null)");
        printf("  MINUTES = 0x%016llx\n",
               (unsigned long long)T[i].tm.MINUTES);
        printf("  HOURS   = 0x%08x\n", T[i].tm.HOURS);
        printf("  DOW     = 0x%02x\n", T[i].tm.DAYSOFWEEK);
    }

    free(T);

    //fin de TEST pour read_tasks()

    /*
    int fd_request = open(PATH1, O_RDONLY); //lire la requete du client 
    if (fd_request == -1) {exit(1) ; } 
    int fd_reply = open(PATH2, O_WRONLY) ;  // ecrire la reponse pour le client 
    if (fd_reply == -1) {exit(1) ; } 

    */
  
}


