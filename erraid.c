#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

char chemin[256] , PATH1[256], PATH2[256] ; 

//initialiser les tubes de communications pour l'instant sans option -p 
void init_pipes() {
    
    
    const char * user = getenv("USER") ; 
    snprintf (chemin, sizeof chemin , "/tmp/%s/erraid/", user) ; 
    if (mkdir(chemin, 0700) == -1 && errno != EEXIST) {
        perror("mkdir");
    }
    char chemin2[256] ; 
    snprintf(chemin2 ,sizeof chemin2, "%s/pipes/", chemin) ; 
    if (mkdir(chemin2, 0700) == -1 && errno != EEXIST) {
        perror("mkdir");
    }

    snprintf (PATH1, sizeof PATH1 , "%serraid-request-pipe", chemin2) ; 
    snprintf(PATH2 , sizeof PATH2, "%serraid-reply-pipe", chemin2) ; 
    if (mkfifo(PATH1, 0666) == -1 && errno != EEXIST) perror ("mkfifo request") ;
    if (mkfifo(PATH2, 0666) == -1 && errno != EEXIST) perror ("mkfifo reply") ; 
}

int main (int argc, char *argv[]) {
   
    init_pipes() ; 

    int fd_request = open(PATH1, O_RDONLY); //lire la requete du client 
    if (fd_request == -1) {exit(1) ; } 
    int fd_reply = open(PATH2, O_WRONLY) ;  // ecrire la reponse pour le client 
    if (fd_reply == -1) {exit(1) ; } 
    
    char buf[512] ; 
    

}


