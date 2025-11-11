#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>




int main(int argc, char *argv[]) {
    
    char PATH1[256] , PATH2[256] ; 
    char * user = getenv("USER") ; 
    snprintf(PATH1 , sizeof PATH1 , "/tmp/%s/erraid/pipes/erraid-request-pipe" , user) ; 
    snprintf(PATH2 , sizeof PATH2 , "/tmp/%s/erraid/pipes/erraid-reply-pipe" , user) ;
    int fd_wr = open (PATH1, O_WRONLY) ; // ecrire une requete pour le demon 
    if (fd_wr == -1) {exit(1) ; }
    int fd_rd = open (PATH2 , O_RDONLY) ; // lire la reponse du demon 
    if (fd_rd == -1) {exit(1) ;}


   




}
