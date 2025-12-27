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
    int start = -1;
    for (int i = 0; i <= max; i++) {
        int is_set = (i < max) && ((val >> i) & 1);
        if (is_set) {
            if (start == -1) start = i;
        } else {
            if (start != -1) {
                if (!first) printf(",");
                if (start == i - 1) {
                    printf("%d", start);
                } else {
                    printf("%d-%d", start, i - 1);
                }
                first = 0;
                start = -1;
            }
        }
    }
    if (first) printf("-");
}

void print_cmd_recursive(int fd, int is_root) {
    uint16_t type = read_u16(fd);

    if (type == TYPE_SIMPLE) {
        uint32_t argc = read_u32(fd);
        for (uint32_t i = 0; i < argc; i++) {
            uint32_t len = read_u32(fd);
            char buf[len + 1];
            read_all(fd, buf, len);
            buf[len] = '\0';
            printf("%s", buf);
            if (i < argc - 1) printf(" ");
        }
    } else if (type == TYPE_SEQ || type == TYPE_PL) {
        uint32_t ncmds = read_u32(fd);
        char *sep = (type == TYPE_SEQ) ? " ; " : " | ";

        if (!is_root) printf("(");
        for (uint32_t i = 0; i < ncmds; i++) {
            print_cmd_recursive(fd, 0);
            if (i < ncmds - 1) printf("%s", sep);
        }
        if (!is_root) printf(")");
    } else if (type == TYPE_IF) {
        uint32_t ncmds = read_u32(fd);
        if (!is_root) printf("(");

        printf("if ");
        print_cmd_recursive(fd, 0);
        printf(" ; then ");

        print_cmd_recursive(fd, 0);

        if (ncmds == 3) {
            printf(" else ");
            print_cmd_recursive(fd, 0);
        }

        printf(" fi");
        if (!is_root) printf(")");
    }
}

void print_cmd(int fd) {
    print_cmd_recursive(fd, 1);
}

int main(int argc, char *argv[]) {
    char chemin_pipes[256], req[256], rep[256];
    char *user = getenv("USER");
    snprintf(chemin_pipes, sizeof chemin_pipes, "/tmp/%s/erraid/pipes", user);
    
    // Initialisation par défaut si user est NULL (sécurité)
    if (!user) snprintf(chemin_pipes, sizeof chemin_pipes, "/tmp/erraid/pipes");

    int opt, fd_req, fd_rep;
    
    // Ajout de 'c' dans la chaîne d'options
    while ((opt = getopt(argc, argv, "lP:x:o:e:qr:c")) != -1) {
        switch (opt) {
        case 'P':
            snprintf(chemin_pipes, sizeof(chemin_pipes), "%s", optarg);
            snprintf(req, sizeof(req), "%s/erraid-request-pipe", chemin_pipes);
            snprintf(rep, sizeof(rep), "%s/erraid-reply-pipe", chemin_pipes);
            break;

        case 'l':
            snprintf(req, sizeof(req), "%s/erraid-request-pipe", chemin_pipes);
            snprintf(rep, sizeof(rep), "%s/erraid-reply-pipe", chemin_pipes);

            fd_req = open(req, O_WRONLY);
            if (fd_req == -1) { perror("open req"); exit(1); }
            uint16_t opcode = htobe16(0x4C53);
            if (write(fd_req, &opcode, sizeof(uint16_t)) == -1) {
                perror("write req");
                exit(1);
            }
            close(fd_req);

            fd_rep = open(rep, O_RDONLY);
            if (fd_rep == -1) { perror("open rep"); exit(1); }

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
                    if (min == 0 && hour == 0 && day == 0) {
                        printf("- - - ");
                    } else {
                        print_timing(min, 60); printf(" ");
                        print_timing((uint64_t)hour, 24); printf(" ");
                        print_timing((uint64_t)day, 7); printf(" ");
                    }
                    print_cmd(fd_rep);
                    printf("\n");
                }
            } else {
                fprintf(stderr, "Erreur du demon: 0x%x\n", reponse);
            }
            close(fd_rep);
            break;

        case 'c': {
            // Initialisation des chemins (au cas où -P n'a pas été appelé avant)
            snprintf(req, sizeof(req), "%s/erraid-request-pipe", chemin_pipes);
            snprintf(rep, sizeof(rep), "%s/erraid-reply-pipe", chemin_pipes);

            // Vérification des arguments min, hour, day + au moins 1 cmd
            if (optind + 3 >= argc) {
                fprintf(stderr, "Usage: %s -c <min> <hour> <day> <cmd> [args...]\n", argv[0]);
                exit(EXIT_FAILURE);
            }

            // Parsing du timing
            uint64_t min = strtoull(argv[optind], NULL, 10);
            uint32_t hour = (uint32_t)strtoul(argv[optind + 1], NULL, 10);
            uint8_t day = (uint8_t)strtoul(argv[optind + 2], NULL, 10);

            // Parsing de la commande
            int cmd_start_idx = optind + 3;
            int cmd_argc = argc - cmd_start_idx;

            // Connexion et envoi
            fd_req = open(req, O_WRONLY);
            if (fd_req == -1) { perror("open req"); exit(1); }

            // Envoi Header CREATE (0x4352) + Timing
            write_u16(fd_req, 0x4352); 
            write_u64(fd_req, min);
            write_u32(fd_req, hour);
            write(fd_req, &day, 1);

            // Envoi Commande (TYPE_SIMPLE / 0x5349)
            write_u16(fd_req, 0x5349);
            write_u32(fd_req, cmd_argc);

            for (int i = 0; i < cmd_argc; i++) {
                char *arg = argv[cmd_start_idx + i];
                uint32_t len = strlen(arg);
                write_u32(fd_req, len);
                write(fd_req, arg, len);
            }
            close(fd_req);

            // Lecture Réponse
            fd_rep = open(rep, O_RDONLY);
            if (fd_rep == -1) { perror("open rep"); exit(1); }

            uint16_t status = read_u16(fd_rep);
            if (status == 0x4F4B) { // OK
                uint64_t new_id = read_u64(fd_rep);
                printf("Tache creee avec succes. ID: %llu\n", (unsigned long long)new_id);
            } else {
                fprintf(stderr, "Erreur lors de la creation (Code 0x%x)\n", status);
            }
            close(fd_rep);
            exit(0); // On quitte après la création
        }

        case 'x': {
            snprintf(req, sizeof(req), "%s/erraid-request-pipe", chemin_pipes);
            snprintf(rep, sizeof(rep), "%s/erraid-reply-pipe", chemin_pipes);
            uint64_t id = strtoull(optarg, NULL, 10);

            fd_req = open(req, O_WRONLY);
            if (fd_req == -1) { perror("Erreur ouverture pipe requete"); exit(1); }

            write_u16(fd_req, 0x5458);
            write_u64(fd_req, id);
            close(fd_req);

            fd_rep = open(rep, O_RDONLY);
            if (fd_rep == -1) { perror("Erreur ouverture pipe reponse"); exit(1); }

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
                exit(1);
            }
            close(fd_rep);
            break;
        }

        case 'o': {
            snprintf(req, sizeof(req), "%s/erraid-request-pipe", chemin_pipes);
            snprintf(rep, sizeof(rep), "%s/erraid-reply-pipe", chemin_pipes);
            uint64_t id = strtoull(optarg, NULL, 10);

            fd_req = open(req, O_WRONLY);
            if (fd_req == -1) { perror("Erreur ouverture pipe requete"); exit(1); }
            write_u16(fd_req, 0x534F);
            write_u64(fd_req, id);
            close(fd_req);

            fd_rep = open(rep, O_RDONLY);
            if (fd_rep == -1) { perror("Erreur ouverture pipe reponse"); exit(1); }

            uint16_t status = read_u16(fd_rep);
            if (status == 0x4F4B) {
                uint32_t taille_fichier = read_u32(fd_rep);
                char buffer[1024];
                uint32_t total_lu = 0;
                while (total_lu < taille_fichier) {
                    int a_lire = 1024;
                    if (taille_fichier - total_lu < 1024) a_lire = taille_fichier - total_lu;
                    int n = read(fd_rep, buffer, a_lire);
                    if (n <= 0) break;
                    write(1, buffer, n);
                    total_lu += n;
                }
            } else {
                fprintf(stderr, "Erreur : Tache inconnue ou fichier vide.\n");
                exit(1);
            }
            close(fd_rep);
            break;
        }

        case 'e': {
            snprintf(req, sizeof(req), "%s/erraid-request-pipe", chemin_pipes);
            snprintf(rep, sizeof(rep), "%s/erraid-reply-pipe", chemin_pipes);
            uint64_t id = strtoull(optarg, NULL, 10);

            fd_req = open(req, O_WRONLY);
            if (fd_req == -1) { perror("Erreur ouverture pipe requete"); exit(1); }
            write_u16(fd_req, 0x5345);
            write_u64(fd_req, id);
            close(fd_req);

            fd_rep = open(rep, O_RDONLY);
            if (fd_rep == -1) { perror("Erreur ouverture pipe reponse"); exit(1); }

            uint16_t status = read_u16(fd_rep);
            if (status == 0x4F4B) {
                uint32_t taille_fichier = read_u32(fd_rep);
                char buffer[1024];
                uint32_t total_lu = 0;
                while (total_lu < taille_fichier) {
                    int a_lire = 1024;
                    if (taille_fichier - total_lu < 1024) a_lire = taille_fichier - total_lu;
                    int n = read(fd_rep, buffer, a_lire);
                    if (n <= 0) break;
                    write(1, buffer, n);
                    total_lu += n;
                }
            } else {
                fprintf(stderr, "Erreur : Tache inconnue ou fichier vide.\n");
                exit(1);
            }
            close(fd_rep);
            break;
        }

        case 'r': {
            snprintf(req, sizeof(req), "%s/erraid-request-pipe", chemin_pipes);
            snprintf(rep, sizeof(rep), "%s/erraid-reply-pipe", chemin_pipes);
            uint64_t id = strtoull(optarg, NULL, 10);

            fd_req = open(req, O_WRONLY);
            if (fd_req == -1) { perror("Erreur ouverture pipe requete"); exit(1); }
            write_u16(fd_req, 0x524D);
            write_u64(fd_req, id);
            close(fd_req);

            fd_rep = open(rep, O_RDONLY);
            if (fd_rep == -1) { perror("Erreur ouverture pipe reponse"); exit(1); }

            uint16_t status = read_u16(fd_rep);
            if (status == 0x4F4B) {
                printf("Tâche %llu supprimée avec succès.\n", (unsigned long long)id);
            } else {
                uint16_t err = read_u16(fd_rep);
                if (err == 0x4E46) fprintf(stderr, "Erreur : Tâche %llu introuvable (NF).\n", (unsigned long long)id);
                else fprintf(stderr, "Erreur : Code 0x%x\n", err);
            }
            close(fd_rep);
            break;
        }

        case 'q': {
            snprintf(req, sizeof(req), "%s/erraid-request-pipe", chemin_pipes);
            snprintf(rep, sizeof(rep), "%s/erraid-reply-pipe", chemin_pipes);

            fd_req = open(req, O_WRONLY);
            if (fd_req == -1) { perror("open req"); exit(1); }
            write_u16(fd_req, 0x4b49);
            close(fd_req);

            fd_rep = open(rep, O_RDONLY);
            if (read_u16(fd_rep) == 0x4F4B) {
                printf("Démon arrêté avec succès.\n");
            }
            close(fd_rep);
            break;
        }

        case '?':
            fprintf(stderr, "Usage: %s [-P PIPES_DIR] [-l] [-x id] [-o id] [-e id] [-r id] [-c min hour day cmd ...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}