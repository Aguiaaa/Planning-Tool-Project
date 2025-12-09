#ifndef ERRAID_H    
#define  ERRAID_H   

#define _DEFAULT_SOURCE   

#include <stdint.h>
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
#include <stdbool.h>
#include <sys/wait.h>  
#include <stdint.h>
#include <sys/select.h>
#include <endian.h>
#include <signal.h>



#define TYPE_SIMPLE  0x5349 // 'SI'
#define TYPE_SEQ     0x5351 // 'SQ'


typedef struct {
    uint32_t LENGTH ; 
    uint8_t * DATA ;
} string ;

typedef struct {
    uint64_t  MINUTES ; 
    uint32_t HOURS ; 
    uint8_t DAYSOFWEEK  ; 
} timing ; 

typedef struct {
   uint32_t ARGC  ; 
   string * ARGV ; 
} arguments ; 

typedef struct command{
    uint16_t type;          
    union {
        arguments args;      
        struct {             
            uint32_t ncmds;  
            struct command *sous_command; 
        } combinaison;           
    };
} command;

typedef struct {
    uint64_t ID ; 
    string chemin ; 
    timing tm ; 
    command * commandes ;
} tasks ; 

#endif