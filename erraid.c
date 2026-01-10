#define _XOPEN_SOURCE 700

#include "erraid.h"
#include "task_runner.h"
#include "parsing_tasks.h"
#include "protocole.h"
#include <signal.h>
#include <time.h>
#include <errno.h>

extern char *optarg;
extern int optind;
char req[256];
char rep[256];

volatile sig_atomic_t running = 1;
volatile sig_atomic_t timer_expired = 0;

void handler_arret(int sig) {
    (void)sig;
    running = 0;
}

void handler_timer(int sig) {
    (void)sig;
    timer_expired = 1;
}

int cp_file(const char *src, const char *dst) {
    int f1 = open(src, O_RDONLY);
    if (f1 == -1) return -1;
    
    int f2 = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (f2 == -1) {
        close(f1);
        return -1;
    }

    char buf[4096];
    ssize_t n;
    while ((n = read(f1, buf, sizeof(buf))) > 0) {
        if (write(f2, buf, n) != n) {
            close(f1);
            close(f2);
            return -1;
        }
    }
    close(f1);
    close(f2);
    return 0;
}

int copy_recursive(const char *src, const char *dst) {
    DIR *d = opendir(src);
    if (!d) return -1;

    if (mkdir(dst, 0700) == -1 && errno != EEXIST) {
        closedir(d);
        return -1;
    }

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;

        char p_src[512], p_dst[512];
        snprintf(p_src, sizeof(p_src), "%s/%s", src, e->d_name);
        snprintf(p_dst, sizeof(p_dst), "%s/%s", dst, e->d_name);

        struct stat st;
        if (stat(p_src, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                copy_recursive(p_src, p_dst);
            } else if (S_ISREG(st.st_mode)) {
                cp_file(p_src, p_dst);
            }
        }
    }
    closedir(d);
    return 0;
}

int rm_dir(const char *path) {
    DIR *d = opendir(path);
    size_t path_len = strlen(path);
    int r = -1;

    if (d) {
        struct dirent *p;
        r = 0;
        while (!r && (p = readdir(d))) {
            int r2 = -1;
            char *buf;
            size_t len;

            if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue;

            len = path_len + strlen(p->d_name) + 2;
            buf = malloc(len);

            if (buf) {
                struct stat statbuf;
                snprintf(buf, len, "%s/%s", path, p->d_name);
                if (!stat(buf, &statbuf)) {
                    if (S_ISDIR(statbuf.st_mode)) r2 = rm_dir(buf);
                    else r2 = unlink(buf);
                }
                free(buf);
            }
            r = r2;
        }
        closedir(d);
    }

    if (!r) r = rmdir(path);
    return r;
}

void write_cmd_recursive(int fd, command *cmd) {
    write_16(fd, cmd->type);
    if (cmd->type == TYPE_SIMPLE) {
        write_32(fd, cmd->args.ARGC);
        for (uint32_t i = 0; i < cmd->args.ARGC; i++) {
            write_32(fd, cmd->args.ARGV[i].LENGTH);
            write(fd, cmd->args.ARGV[i].DATA, cmd->args.ARGV[i].LENGTH);
        }
    } else {
        write_32(fd, cmd->combinaison.ncmds);
        for (uint32_t i = 0; i < cmd->combinaison.ncmds; i++) {
            write_cmd_recursive(fd, &cmd->combinaison.sous_command[i]);
        }
    }
}

void traiter_remove(int fd_req, char *rep_path, char *base_path, tasks **T, uint64_t *nbtasks) {
    uint64_t id_be, id;
    if (read_all(fd_req, &id_be, 8) < 0) return;
    id = be64toh(id_be);

    printf("Suppression ID %llu\n", (unsigned long long)id);

    int index = -1;
    for (uint64_t i = 0; i < *nbtasks; i++) {
        if ((*T)[i].ID == id) {
            index = i;
            break;
        }
    }

    int fd_rep = open(rep_path, O_WRONLY);
    if (fd_rep == -1) return;

    if (index == -1) {
        write_16(fd_rep, REP_ERR);
        write_16(fd_rep, 0x4E46);
    } else {
        char dir_path[512];
        snprintf(dir_path, sizeof(dir_path), "%s/%llu", base_path, (unsigned long long)id);
        rm_dir(dir_path);

        if (index != *nbtasks - 1) {
            (*T)[index] = (*T)[*nbtasks - 1];
        }
        (*nbtasks)--;

        write_16(fd_rep, REP_OK);
    }
    close(fd_rep);
}

void traiter_xoe(char *rep_path, char *base_path, uint64_t id, char *filename) {
    int fd_rep = open(rep_path, O_WRONLY);
    if (fd_rep == -1) return;

    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/%llu/%s", base_path, (unsigned long long)id, filename);

    int fd_file = open(file_path, O_RDONLY);
    if (fd_file == -1) {
        write_16(fd_rep, REP_ERR);
        char dir_path[512];
        snprintf(dir_path, sizeof(dir_path), "%s/%llu", base_path, (unsigned long long)id);
        struct stat st_task;
        if (stat(dir_path, &st_task) == -1) write_16(fd_rep, 0x4E46); 
        else write_16(fd_rep, 0x4E52); 
        close(fd_rep);
        return;
    }

    write_16(fd_rep, REP_OK);
    struct stat st;
    fstat(fd_file, &st);
    
    if (strcmp(filename, "times-exitcodes") == 0) {
        write_32(fd_rep, (uint32_t)(st.st_size / 10));
    } else {
        write_32(fd_rep, (uint32_t)st.st_size);
    }

    char buf[1024];
    ssize_t n;
    while ((n = read(fd_file, buf, sizeof(buf))) > 0) {
        write(fd_rep, buf, n);
    }

    close(fd_file);
    close(fd_rep);
}

void lister_taches(char *rep_path, tasks *T, uint64_t nbtasks) {
    int fd_rep = open(rep_path, O_WRONLY);
    if (fd_rep == -1) return;

    write_16(fd_rep, REP_OK);
    write_32(fd_rep, (uint32_t)nbtasks);

    for (uint64_t i = 0; i < nbtasks; i++) {
        write_64(fd_rep, T[i].ID);
        write_64(fd_rep, T[i].tm.MINUTES);
        write_32(fd_rep, T[i].tm.HOURS);
        write(fd_rep, &T[i].tm.DAYSOFWEEK, 1);
        write_cmd_recursive(fd_rep, T[i].commandes);
    }
    close(fd_rep);
}

uint64_t get_next_id(tasks *T, uint64_t nbtasks) {
    uint64_t max_id = 0;
    if (nbtasks == 0) return 1;
    for (uint64_t i = 0; i < nbtasks; i++) {
        if (T[i].ID > max_id) max_id = T[i].ID;
    }
    return max_id + 1;
}

int save_cmd_recursive(int fd_req, char *dir_path) {
    if (mkdir(dir_path, 0700) == -1 && errno != EEXIST) return -1;

    uint16_t type_be = read_16(fd_req); 
    char path_type[512];
    snprintf(path_type, sizeof(path_type), "%s/type", dir_path);
    int fd_type = open(path_type, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    write_16(fd_type, type_be); 
    close(fd_type);

    if (type_be == TYPE_SIMPLE) {
        uint32_t argc = read_32(fd_req);
        char path_argv[512];
        snprintf(path_argv, sizeof(path_argv), "%s/argv", dir_path);
        int fd_argv = open(path_argv, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        write_32(fd_argv, argc); 
        for (uint32_t i = 0; i < argc; i++) {
            uint32_t len = read_32(fd_req);
            write_32(fd_argv, len);         
            char *buf = malloc(len + 1);
            read_all(fd_req, buf, len);     
            write(fd_argv, buf, len);       
            free(buf);
        }
        close(fd_argv);
    } else {
        uint32_t ncmds = read_32(fd_req);
        for (uint32_t i = 0; i < ncmds; i++) {
            char sub_path[512];
            snprintf(sub_path, sizeof(sub_path), "%s/%u", dir_path, i);
            save_cmd_recursive(fd_req, sub_path);
        }
    }
    return 0;
}

void traiter_create(int fd_req, char *rep_path, char *base_path, tasks **T, uint64_t *nbtasks) {
    printf("Creation (CR)\n");
    uint64_t new_id = get_next_id(*T, *nbtasks);
    
    char task_dir[512];
    snprintf(task_dir, sizeof(task_dir), "%s/%llu", base_path, (unsigned long long)new_id);
    mkdir(task_dir, 0700);
    
    uint64_t min = read_64(fd_req);
    uint32_t hour = read_32(fd_req);
    uint8_t day;
    read_all(fd_req, &day, 1);

    char path_timing[512];
    snprintf(path_timing, sizeof(path_timing), "%s/timing", task_dir);
    int fd_time = open(path_timing, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    write_64(fd_time, min);
    write_32(fd_time, hour);
    write(fd_time, &day, 1);
    close(fd_time);

    char path_cmd[512];
    snprintf(path_cmd, sizeof(path_cmd), "%s/cmd", task_dir);
    save_cmd_recursive(fd_req, path_cmd);

    if (*T) free_tasks(*T, *nbtasks);
    *T = read_tasks(base_path);
    *nbtasks = number_of_tasks(base_path);

    int fd_rep = open(rep_path, O_WRONLY);
    if (fd_rep != -1) {
        write_16(fd_rep, REP_OK);
        write_64(fd_rep, new_id);
        close(fd_rep);
    }
}

void traiter_combine(int fd_req, char *rep_path, char *base_path, tasks **T, uint64_t *nbtasks) {
    printf("Combinaison (CB)\n");

    read_16(fd_req); 

    uint64_t min = read_64(fd_req);
    uint32_t hour = read_32(fd_req);
    uint8_t day;
    read_all(fd_req, &day, 1);

    uint16_t type_comb = read_16(fd_req);
    uint32_t count = read_32(fd_req);

    if (type_comb == TYPE_IF && (count < 2 || count > 3)) {
        fprintf(stderr, "Erreur IF: %d args\n", count);
        for(uint32_t i=0; i<count; i++) {
            uint64_t t;
            read_all(fd_req, &t, 8);
        }
        int fd_rep = open(rep_path, O_WRONLY);
        if (fd_rep != -1) {
            write_16(fd_rep, REP_ERR);
            write_16(fd_rep, 0x4E46);
            close(fd_rep);
        }
        return;
    }

    uint64_t *ids = malloc(count * sizeof(uint64_t));
    for (uint32_t i = 0; i < count; i++) {
        uint64_t tmp;
        read_all(fd_req, &tmp, 8);
        ids[i] = be64toh(tmp);
    }

    uint64_t new_id = get_next_id(*T, *nbtasks);
    char task_dir[512];
    snprintf(task_dir, sizeof(task_dir), "%s/%llu", base_path, (unsigned long long)new_id);
    mkdir(task_dir, 0700);

    char path_timing[512];
    snprintf(path_timing, sizeof(path_timing), "%s/timing", task_dir);
    int fd_time = open(path_timing, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    write_64(fd_time, min);
    write_32(fd_time, hour);
    write(fd_time, &day, 1);
    close(fd_time);

    char cmd_dir[512];
    snprintf(cmd_dir, sizeof(cmd_dir), "%s/cmd", task_dir);
    mkdir(cmd_dir, 0700);

    char path_type[512];
    snprintf(path_type, sizeof(path_type), "%s/type", cmd_dir);
    int fd_type = open(path_type, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    write_16(fd_type, type_comb); 
    close(fd_type);

    for (uint32_t i = 0; i < count; i++) {
        char src_cmd[512];
        snprintf(src_cmd, sizeof(src_cmd), "%s/%llu/cmd", base_path, (unsigned long long)ids[i]);
        char dst_subdir[512];
        snprintf(dst_subdir, sizeof(dst_subdir), "%s/%u", cmd_dir, i); 
        copy_recursive(src_cmd, dst_subdir);
    }

    for (uint32_t i = 0; i < count; i++) {
        char old_dir[512];
        snprintf(old_dir, sizeof(old_dir), "%s/%llu", base_path, (unsigned long long)ids[i]);
        rm_dir(old_dir);
    }

    free(ids);
    if (*T) free_tasks(*T, *nbtasks);
    *T = read_tasks(base_path);
    *nbtasks = number_of_tasks(base_path);

    int fd_rep = open(rep_path, O_WRONLY);
    if (fd_rep != -1) {
        write_16(fd_rep, REP_OK);
        write_64(fd_rep, new_id);
        close(fd_rep);
    }
}

int main(int argc, char *argv[]) {
    char chemin_tasks[256], chemin_pipes[256];
    char *user = getenv("USER");
    char *arg_r = NULL, *arg_p = NULL;
    int avantPlan = 0;
    int opt;

    while ((opt = getopt(argc, argv, "r:R:p:P:F")) != -1) {
        switch (opt) {
            case 'r': case 'R': arg_r = optarg; break;
            case 'p': case 'P': arg_p = optarg; break;
            case 'F': avantPlan = 1; break;
            default: exit(EXIT_FAILURE);
        }
    }
    
    if (arg_r) snprintf(chemin_tasks, sizeof(chemin_tasks), "%s/tasks", arg_r);
    else if (user) snprintf(chemin_tasks, sizeof(chemin_tasks), "/tmp/%s/erraid/tasks", user);
    else strcpy(chemin_tasks, "/tmp/erraid/tasks");

    if (arg_p) snprintf(chemin_pipes, sizeof(chemin_pipes), "%s", arg_p);
    else if (arg_r) snprintf(chemin_pipes, sizeof(chemin_pipes), "%s/pipes", arg_r);
    else if (user) snprintf(chemin_pipes, sizeof(chemin_pipes), "/tmp/%s/erraid/pipes", user);
    else strcpy(chemin_pipes, "/tmp/erraid/pipes");

    snprintf(req, sizeof req, "%s/erraid-request-pipe", chemin_pipes);
    snprintf(rep, sizeof rep, "%s/erraid-reply-pipe", chemin_pipes);

    if (!arg_p && !arg_r && user) {
        char parent[256];
        snprintf(parent, sizeof(parent), "/tmp/%s/erraid", user);
        mkdir(parent, 0700);
    }
    struct stat st;
    if (stat(chemin_pipes, &st) == -1) mkdir(chemin_pipes, 0700);
    mkfifo(req, 0622);
    mkfifo(rep, 0622);
    
    if (!avantPlan) {
        if (fork() > 0) exit(0);
        setsid();
        if (fork() > 0) exit(0);
        close(1) ; close(0) ; close(2) ; 
    }

    struct sigaction sa = { 0 };
    sa.sa_handler = handler_timer;
    sigaction(SIGALRM, &sa, NULL);

    struct sigaction sa_int = { 0 };
    sa_int.sa_handler = handler_arret;
    sigaction(SIGINT, &sa_int, NULL);
    sigaction(SIGTERM, &sa_int, NULL);

    int fd_req = open(req, O_RDWR);
    tasks *T = read_tasks(chemin_tasks);
    uint64_t nbtasks = number_of_tasks(chemin_tasks);

    while (running) {
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        
        int sec_to_wait = 60 - tm_now.tm_sec;
        if (sec_to_wait <= 0) sec_to_wait = 1;
        
        if (timer_expired) { 
            timer_expired = 0;
            time_t t2 = time(NULL);
            struct tm tm2;
            localtime_r(&t2, &tm2);
                    
            for (uint64_t i = 0; i < nbtasks; i++) {
                if (should_run(&T[i], &tm2)) {
                    if (fork() == 0) {
                        if (fork() == 0) {
                                execute_task(&T[i]);
                                exit(0);
                        }
                        exit(0);
                    }
                     wait(NULL);
                }
            }
        }

        alarm(sec_to_wait); 

        uint16_t opcode_be;
        ssize_t n = read(fd_req, &opcode_be, 2);

        alarm(0); 

        if (n == -1) {
            if (errno == EINTR) {
                if (timer_expired) {
                    timer_expired = 0;
                    time_t t2 = time(NULL);
                    struct tm tm2;
                    localtime_r(&t2, &tm2);
                    
                    for (uint64_t i = 0; i < nbtasks; i++) {
                        if (should_run(&T[i], &tm2)) {
                            if (fork() == 0) {
                                if (fork() == 0) {
                                    execute_task(&T[i]);
                                    exit(0);
                                }
                                exit(0);
                            }
                            wait(NULL);
                        }
                    }
                }
                continue;
            }
            perror("read");
            break;
        }

        if (n == 0) continue;

        uint16_t opcode = be16toh(opcode_be);
        if (opcode == LIST) lister_taches(rep, T, nbtasks);
        else if (opcode == 0x4352) traiter_create(fd_req, rep, chemin_tasks, &T, &nbtasks);
        else if (opcode == COMBINE) traiter_combine(fd_req, rep, chemin_tasks, &T, &nbtasks);
        else if (opcode == 0x524D) traiter_remove(fd_req, rep, chemin_tasks, &T, &nbtasks);
        else if (opcode == STDOUT) {
             uint64_t tmp;
             read_all(fd_req, &tmp, 8);
             traiter_xoe(rep, chemin_tasks, be64toh(tmp), "stdout");
        }
        else if (opcode == STDERR) {
             uint64_t tmp;
             read_all(fd_req, &tmp, 8);
             traiter_xoe(rep, chemin_tasks, be64toh(tmp), "stderr");
        }
        else if (opcode == TIMES_EXITCODES) {
             uint64_t tmp;
             read_all(fd_req, &tmp, 8);
             traiter_xoe(rep, chemin_tasks, be64toh(tmp), "times-exitcodes");
        }
        else if (opcode == TERM) {
            running = 0;
            int f = open(rep, O_WRONLY);
            if(f != -1) {
                write_16(f, REP_OK);
                close(f);
            }
        }
    }

    close(fd_req);
    unlink(req);
    unlink(rep);
    if(T) free_tasks(T, nbtasks);
    return 0;
}