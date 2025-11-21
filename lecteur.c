#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>
#include "erraid.h"
#include "lecteur.h"

/* Auteur du module lecteur: Amine */

struct dirent *entry , *entry2 ; 
struct stat st;

void print_command(const command *cmd) {
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
        (void)chemin;
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
            printf("nombres de sous comamndes : %lu\n", (unsigned long)number_of_cmd);
            closedir(d_cmd) ; 
            c->combinaison.ncmds = (uint32_t) number_of_cmd ; 
            c->combinaison.sous_command = malloc(number_of_cmd * sizeof(command));
            d_cmd = opendir(chemin_cmd) ; 

            while ((entry2 = readdir (d_cmd)) != NULL ) {
                if (strcmp(entry2->d_name , ".") != 0 && strcmp(entry2->d_name , "..") != 0) {  
                char chemin_sous_cmd [256]; 
                snprintf(chemin_sous_cmd, sizeof chemin_sous_cmd, "%s/%s", chemin_cmd, entry2->d_name); 
                if (stat(chemin_sous_cmd, &st) == 0 && S_ISDIR(st.st_mode)) {
                    uint32_t i = (uint32_t)strtoul(entry2->d_name, NULL, 10);
                    if (i < c->combinaison.ncmds) {
                        read_cmd(&(c->combinaison.sous_command[i]) , chemin_sous_cmd, chemin_sous_cmd); 
                    }
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
            
            uint32_t length = (uint32_t) (strlen(chemin) + strlen(entry->d_name) + 1) ; 
            tache.chemin.DATA = malloc(length + 1);
            memcpy(tache.chemin.DATA, chemin_tache, length + 1);
            printf("chemin tache : %s\n ", tache.chemin.DATA) ;
            tache.chemin.LENGTH = length ;  
            
            char chemin_fichier_timing[256] ;
            snprintf (chemin_fichier_timing , sizeof chemin_fichier_timing , "%s/timing" , chemin_tache ) ;  
            int fd = open(chemin_fichier_timing , O_RDONLY) ; 
            if (fd == -1) {perror("erreur d'ouverture de timing" ) ;closedir(dir);return NULL ; }
            uint64_t minutes ; uint32_t hours ; uint8_t daysofweek ;  
            ssize_t n ; 
            n = read (fd , &minutes , 8) ; 
            if (n != 8) {close(fd) ; perror("erreur read timing") ; closedir(dir);return NULL;  }
            n = read (fd , &hours , 4) ;
            if (n != 4) {close(fd) ; perror("erreur read timing") ; closedir(dir);return NULL ;  }
            n = read (fd , &daysofweek, 1) ; 
            if (n != 1) {close(fd) ; perror("erreur read timing") ; closedir(dir);return NULL; }
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

void free_tasks(tasks *T, uint64_t count) {
    for (uint64_t i = 0; i < count; i++) {
        free(T[i].chemin.DATA);
        free_cmd(T[i].commandes);
        free(T[i].commandes);
    }
    free(T);

}

void free_cmd(command *cmd) {
    if (!cmd) return;
    if (cmd->type == 0x5349) {
        for (uint32_t i = 0; i < cmd->args.ARGC; i++) {
            free(cmd->args.ARGV[i].DATA);
        }
        free(cmd->args.ARGV);
    } else if (cmd->type == 0x5351) {
        for (uint32_t i = 0; i < cmd->combinaison.ncmds; i++) {
            free_cmd(&cmd->combinaison.sous_command[i]);
        }
        free(cmd->combinaison.sous_command);
    }
}
