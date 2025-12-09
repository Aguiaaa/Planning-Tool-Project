#include "tadmor.h"
#include "erraid.h"
#include "protocole.h"

extern char *optarg;

void print_timing(uint64_t val, int max) {
    
    if (val == ((1ULL << max) - 1)) { 
        printf("*"); 
        return; 
    }

    int first = 1;
    for (int i = 0; i < max; i++) {
        if ((val >> i) & 1) { 
            if (!first) printf(",");
            printf("%d", i);
            first = 0;
        }
    }
    if (first) printf("-"); 
}

void print_cmd(int fd) {
    uint16_t type = read_u16(fd);

    if (type == 0x5349) { 
        uint32_t argc = read_u32(fd);
        for (uint32_t i = 0; i < argc; i++) {
            uint32_t len = read_u32(fd);
            char buf[len + 1];
            read_all(fd, buf, len);
            buf[len] = '\0';
            printf("%s", buf);
            if (i < argc - 1) printf(" ");
        }
    } 
    else if (type == 0x5351) {
        uint32_t ncmds = read_u32(fd);
        printf("(");
        for (uint32_t i = 0; i < ncmds; i++) {
            print_cmd(fd);
            if (i < ncmds - 1) printf("; ");
        }
        printf(")");
    }
}

int main(int argc, char *argv[]) {
    
    char chemin_pipes[256] , req[256], rep[256];
    char * user = getenv("USER") ;  
    snprintf (chemin_tasks , sizeof chemin_tasks, "/tmp/%s/erraid/tasks", user) ; 
    snprintf(chemin_pipes, sizeof chemin_pipes, "/tmp/%s/erraid/pipes", user);
    int opt, fd_req , fd_rep ; 
    while ((opt = getopt(argc, argv, "lp:x:o:e:")) != -1) {
        switch (opt) {
            case 'p':
                snprintf(chemin_pipes, sizeof(chemin_pipes), "%s", optarg);
                snprintf(req, sizeof(req), "%s/erraid-request-pipe", chemin_pipes);
                snprintf(rep, sizeof(rep), "%s/erraid-reply-pipe", chemin_pipes);
                break;

            case 'l':
                fd_req = open(req, O_WRONLY);
                uint16_t opcode = htobe16(0x4C53);
                if (write(fd_req, &opcode, sizeof(uint16_t)) == -1) {
                    perror("write req");
                    exit(1);
                }
                close(fd_req);
                
                fd_rep = open(rep, O_RDONLY);
                if (fd_rep == -1) {
                    perror("open rep");
                    exit(1);
                }

                uint16_t reponse = read_u16(fd_rep);
                if (reponse == 0x4F4B) {
                    uint32_t nbtasks = read_u32(fd_rep);

                    for (uint32_t i = 0; i < nbtasks; i++) {
                        uint64_t id = read_u64(fd_rep);

                        uint64_t min = read_u64(fd_rep);
                        uint32_t hour = read_u32(fd_rep);
                        uint8_t day;
                        read_all(fd_rep, &day, 1);
                        
                        printf("%llu: ", (unsigned long long)id);
                        print_timing(min, 60);
                        printf(" ");
                        print_timing((uint64_t)hour, 24);
                        printf(" ");
                        print_timing((uint64_t)day, 7);
                        printf(" ");
                        print_cmd(fd_rep);
                        printf("\n");
                    }
                } else {
                    fprintf(stderr, "Erreur du demon: 0x%x\n", reponse);
                }
                close(fd_rep);
                break;

            case 'x': {
                uint64_t id = strtoull(optarg, NULL, 10);

                fd_req = open(req, O_WRONLY);
                if (fd_req == -1) {
                    perror("Erreur ouverture pipe requete");
                    exit(1);
                }

                write_u16(fd_req, 0x5458);
                write_u64(fd_req, id);
                close(fd_req);

                fd_rep = open(rep, O_RDONLY);
                if (fd_rep == -1) {
                    perror("Erreur ouverture pipe reponse");
                    exit(1);
                }

                uint16_t status = read_u16(fd_rep);

                if (status == 0x4F4B) {
                    uint32_t nombre_execs = read_u32(fd_rep);

                    for (uint32_t i = 0; i < nombre_execs; i++) {
                        uint64_t timestamp = read_u64(fd_rep);
                        uint16_t code_retour = read_u16(fd_rep);

                        time_t t = (time_t)timestamp;
                        struct tm *info = localtime(&t);

                        char date_lisible[64];
                        strftime(date_lisible, 64, "%Y-%m-%d %H:%M:%S", info);

                        printf("%s %d\n", date_lisible, code_retour);
                    }
                } else {
                    fprintf(stderr, "Erreur : La tache n'existe pas ou n'a pas d'historique.\n");
                }

                close(fd_rep);
                break;
            }

            case 'o': {
                uint64_t id = strtoull(optarg, NULL, 10);

                fd_req = open(req, O_WRONLY);
                if (fd_req == -1) {
                    perror("Erreur ouverture pipe requete");
                    exit(1);
                }

                write_u16(fd_req, 0x534F);
                write_u64(fd_req, id);
                close(fd_req);

                fd_rep = open(rep, O_RDONLY);
                if (fd_rep == -1) {
                    perror("Erreur ouverture pipe reponse");
                    exit(1);
                }

                uint16_t status = read_u16(fd_rep);

                if (status == 0x4F4B) {
                    uint32_t taille_fichier = read_u32(fd_rep);

                    char buffer[1024];
                    uint32_t total_lu = 0;

                    while (total_lu < taille_fichier) {
                        int a_lire = 1024;
                        if (taille_fichier - total_lu < 1024) {
                            a_lire = taille_fichier - total_lu;
                        }

                        int n = read(fd_rep, buffer, a_lire);
                        if (n <= 0) break;

                        write(1, buffer, n);
                        total_lu += n;
                    }
                } else {
                    fprintf(stderr, "Erreur : Tache inconnue ou fichier vide.\n");
                }

                close(fd_rep);
                break;
            }

            case 'e': {
                uint64_t id = strtoull(optarg, NULL, 10);

                fd_req = open(req, O_WRONLY);
                if (fd_req == -1) {
                    perror("Erreur ouverture pipe requete");
                    exit(1);
                }

                write_u16(fd_req, 0x5345);
                write_u64(fd_req, id);
                close(fd_req);

                fd_rep = open(rep, O_RDONLY);
                if (fd_rep == -1) {
                    perror("Erreur ouverture pipe reponse");
                    exit(1);
                }

                uint16_t status = read_u16(fd_rep);

                if (status == 0x4F4B) {
                    uint32_t taille_fichier = read_u32(fd_rep);

                    char buffer[1024];
                    uint32_t total_lu = 0;

                    while (total_lu < taille_fichier) {
                        int a_lire = 1024;
                        if (taille_fichier - total_lu < 1024) {
                            a_lire = taille_fichier - total_lu;
                        }

                        int n = read(fd_rep, buffer, a_lire);
                        if (n <= 0) break;

                        write(1, buffer, n);
                        total_lu += n;
                    }
                } else {
                    fprintf(stderr, "Erreur : Tache inconnue ou fichier vide.\n");
                }

                close(fd_rep);
                break;
            }

            case '?':
                fprintf(stderr, "Usage: %s [-p PIPES_DIR] [-l ] [-x id] [-o id] [-e id]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

}
