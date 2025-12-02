
#define _DEFAULT_SOURCE
#include "parsing_tasks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>


struct dirent *entry , *entry2 ; 
struct stat st;

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
                c->args.ARGV[i].LENGTH = length ;
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