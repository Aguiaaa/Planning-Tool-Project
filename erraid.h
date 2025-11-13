#ifndef ERRAID_H     // Protection contre les inclusions multiples
#define  ERRAID_H   


#include <stdint.h>




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