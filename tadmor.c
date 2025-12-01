#include "tadmor.h"
#include "erraid.h"


extern char *optarg;

ssize_t read_all(int fd, void *buf, size_t count) {
    size_t total = 0;
    ssize_t n;
    while (total < count) {
        n = read(fd, (char*)buf + total, count - total);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

uint16_t read_u16(int fd) {
    uint16_t v;
    if (read_all(fd, &v, 2) < 0) exit(1);
    return be16toh(v);
}

uint32_t read_u32(int fd) {
    uint32_t v;
    if (read_all(fd, &v, 4) < 0) exit(1);
    return be32toh(v);
}

uint64_t read_u64(int fd) {
    uint64_t v;
    if (read_all(fd, &v, 8) < 0) exit(1);
    return be64toh(v);
}
void print_timing_field(uint64_t val, int max_bits) {
    bool all_set = true;
    for (int i = 0; i < max_bits; i++) {
        if (!((val >> i) & 1)) {
            all_set = false;
            break;
        }
    }

    if (all_set) {
        printf("*");
    } else {
        bool first = true;
        bool any = false;
        for (int i = 0; i < max_bits; i++) {
            if ((val >> i) & 1) {
                if (!first) printf(",");
                printf("%d", i);
                first = false;
                any = true;
            }
        }
        if (!any) printf("-");
    }
}

void print_cmd_recursive(int fd) {
    uint16_t type = read_u16(fd);

    if (type == 0x5349) { 
        uint32_t argc = read_u32(fd);
        for (uint32_t i = 0; i < argc; i++) {
            uint32_t len = read_u32(fd);
            char *buf = malloc(len + 1);
            read_all(fd, buf, len);
            buf[len] = '\0';
            printf("%s", buf);
            free(buf);
            if (i < argc - 1) printf(" ");
        }
    } 
    else if (type == 0x5351) {
        uint32_t ncmds = read_u32(fd);
        printf("(");
        for (uint32_t i = 0; i < ncmds; i++) {
            print_cmd_recursive(fd);
            if (i < ncmds - 1) printf("; ");
        }
        printf(")");
    }
}

int main(int argc, char *argv[]) {
    
    char chemin_pipes[256] , req[256], rep[256];
    
    
   int opt, fd_req , fd_rep ; 
    while ((opt = getopt(argc, argv, "lp:")) != -1) {
        switch (opt) {
            case 'p' :
            snprintf(chemin_pipes, sizeof(chemin_pipes), "%s/pipes", optarg);
            snprintf(req, sizeof(req), "%s/erraid-request-pipe", chemin_pipes);
            snprintf(rep, sizeof(rep), "%s/erraid-reply-pipe", chemin_pipes);
            break ; 

            case 'l':
                fd_req = open (req, O_WRONLY ) ; 
                uint16_t opcode= htobe16(0x4C53);
                if (write(fd_req, &opcode, sizeof(uint16_t)) == -1){
                    perror("write req") ;
                    exit(1) ;
                } 
                close(fd_req) ;
                printf("j'envoie la demande au demon un peu de patience !\n") ; 
                fd_rep = open (rep , O_RDONLY) ;
                if (fd_rep == -1) { perror("open rep"); exit(1); }

                uint16_t anstype = read_u16(fd_rep);
                if (anstype == 0x4F4B) { 
                    uint32_t nbtasks = read_u32(fd_rep);
                    
                    for (uint32_t i = 0; i < nbtasks; i++) {
                        uint64_t id = read_u64(fd_rep);
                        
                        uint64_t min = read_u64(fd_rep);
                        uint32_t hour = read_u32(fd_rep);
                        uint8_t day;
                        read_all(fd_rep, &day, 1);
                        printf("%llu: ", (unsigned long long)id);

                        print_timing_field(min, 60);
                        printf(" ");
                        print_timing_field((uint64_t)hour, 24); 
                        printf(" ");
                        print_timing_field((uint64_t)day, 7); 
                        printf(" ");

                        print_cmd_recursive(fd_rep);
                        printf("\n");
                    }
                } else {
                    fprintf(stderr, "Erreur serveur: 0x%x\n", anstype);
                }

                close(fd_rep);

                
                break;
           
            case '?': 
                fprintf(stderr, "Usage: %s [-l ]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }


}
