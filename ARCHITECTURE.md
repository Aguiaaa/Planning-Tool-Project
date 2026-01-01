# Architecture - erraid & tadmor

## Introduction

Ce projet applique les concepts fondamentaux du cours SY5 :
- **Processus** : création avec `fork()`, recouvrement avec `execvp()`
- **Signaux** : gestion avec `sigaction()` pour SIGALRM (réveil périodique) et SIGINT/SIGTERM (arrêt gracieux)
- **Tubes nommés** : communication inter-processus avec `mkfifo()`
- **Fichiers** : persistence via `open()`, `read()`, `write()`, `stat()`
- **Répertoires** : traversal avec `opendir()`, `readdir()`

---

## 1. Démoniisation - Séparation du processus père

Concept appliqué : **double fork** 

Le démon se détache du terminal via une technique classique :

```c
// Premier fork : crée un processus fils
if (fork() > 0) exit(0);  // Père quitte

// Créer une nouvelle session (dissociation du terminal)
setsid();

// Deuxième fork : garantit qu'on ne peut pas réacquérir de terminal
if (fork() > 0) exit(0);  // Premier fils quitte
// Seul le deuxième fils continue (démon)
```

**Raison** : Éviter que le démon reçoive SIGHUP si le terminal ferme, garantir qu'aucun terminal ne peut être associé au démon.

---

## 2. Gestion des signaux

Concept appliqué : **sigaction()**, masquage de signaux, handlers 

### SIGALRM : Réveil périodique pour exécution des tâches

```c
struct sigaction sa;
sa.sa_handler = handler_timer;     // Fonction dédiée
sigemptyset(&sa.sa_mask);          // Aucun masquage supplémentaire
sa.sa_flags = 0;
sigaction(SIGALRM, &sa, NULL);     // Installation du gestionnaire
```

**Dans la boucle principale :**
```c
// Calculer secondes jusqu'à prochaine minute
int sec_to_wait = 60 - tm_now.tm_sec;
alarm(sec_to_wait);  // Réveille dans N secondes

// read() bloquant sur le pipe
ssize_t n = read(fd_req, &opcode_be, 2);
alarm(0);  // Annuler l'alarme si read() réussit

if (n == -1 && errno == EINTR) {
    // Signal a interrompu le read
    if (timer_expired) {
        // L'alarme a sonné → exécuter les tâches
        timer_expired = 0;
        // Exécuter les tâches du moment
    }
}
```

**Concept clé** : Un appel système bloquant interrompu par un signal retourne -1 avec errno=EINTR. On redémarre après le gestionnaire.

### SIGINT/SIGTERM : Arrêt gracieux

```c
volatile sig_atomic_t running = 1;

void handler_arret(int sig) {
    running = 0;  // Drapeau atomique
}

struct sigaction sa_int;
sa_int.sa_handler = handler_arret;
sigaction(SIGINT, &sa_int, NULL);
sigaction(SIGTERM, &sa_int, NULL);
```

Handler minimal, juste modifier un `volatile sig_atomic_t`. Le nettoyage se fait dans le programme principal.

---

## 3. Communication : Tubes nommés

Concept appliqué : **tubes nommés** 

### Création et accès

```c
// Création des tubes (par le démon au démarrage)
mkfifo(req_pipe, 0622);   // request-pipe
mkfifo(rep_pipe, 0622);   // reply-pipe
```

**Pourquoi des tubes nommés ?** 
- Permettent la communication entre processus **non apparentés** (contrairement aux tubes anonymes `pipe()`)
- Accessibilité contrôlée via les droits du fichier
- Persistance dans le SGF (le tube existe même si aucun processus ne l'utilise)

### Ouverture bloquante

```c
// Démon : ouvre en lecture (bloquant jusqu'à un écrivain)
int fd_req = open(req, O_RDWR);

// Client : ouvre en écriture, envoie, relit réponse
int fd_req = open(req, O_WRONLY);
write(fd_req, &request, size);
close(fd_req);
```

**Synchronisation** : L'ouverture en lecture bloque tant qu'il n'y a pas d'écrivain.

---

## 4. Organisation des processus

Concept appliqué : **fork(), execvp(), wait()** 

### Exécution des tâches : Double fork

```c
// Démon principal
for (chaque tâche à exécuter) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Fils 1 : fork à nouveau
        if (fork() == 0) {
            // Petit-fils : exécute vraiment la tâche
            execute_task(&T[i]);
            exit(0);
        }
        exit(0);  // Fils 1 quitte
    }
    wait(NULL);  // Père attend le fils 1 (plus rapide)
}
```

**Raison** : Éviter les zombies. Le petit-fils est adopté par le processus 1 (init/systemd). Le père n'a plus à faire de `wait()` sur le petit-fils.

### Exécution des commandes : fork + execvp

```c
// Pour une commande simple
pid_t pid = fork();
if (pid == 0) {
    // Fils : remplacer le code par la commande
    char *args[] = {cmd, arg1, arg2, NULL};
    execvp(cmd, args);  // Ne revient pas si succès
    perror("execvp");
    exit(127);
}
int status;
waitpid(pid, &status, 0);  // Père attend le fils
if (WIFEXITED(status)) {
    int code = WEXITSTATUS(status);  // Code retour
}
```

### Pipelines : Redirection avec dup2

```c
// Pour un pipeline cmd1 | cmd2
for (int i = 0; i < n; i++) {
    if (i < n - 1) {
        pipe(pipefd);  // Créer le tube anonyme
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        // Fils : redirection des descripteurs
        if (prev_pipe != -1) {
            dup2(prev_pipe, STDIN_FILENO);   // stdin ← tube précédent
            close(prev_pipe);
        }
        if (i < n - 1) {
            dup2(pipefd[1], STDOUT_FILENO);  // stdout ← tube suivant
            close(pipefd[1]);
            close(pipefd[0]);
        }
        // Exécuter la commande (stdin/stdout redirigés)
        execvp(cmd, args);
        exit(127);
    }
    
    if (prev_pipe != -1) close(prev_pipe);
    if (i < n - 1) {
        prev_pipe = pipefd[0];
        close(pipefd[1]);
    }
}

// Père attend tous les fils
while (wait(NULL) > 0);
```

**Concept clé** : Les tubes anonymes `pipe()` ne sont accessibles que via les descripteurs hérités lors du `fork()` (cours 9). On ferme les descripteurs inutilisés pour éviter les blocages.

---

## 5. Structures de données

### Hiérarchie : Command récursive

```c
typedef struct command {
    uint16_t type;  // TYPE_SIMPLE, TYPE_SEQ, TYPE_PL, TYPE_IF
    union {
        arguments args;         // Si simple
        struct {
            uint32_t ncmds;
            struct command *sous_command;  
        } combinaison;
    };
} command;
```

**Avantage** : Permet d'imbriquer arbitrairement (if → seq → pipe → simple). Une séquence peut contenir un pipeline contenant un if, etc.

### Horaire : Masques de bits

```c
typedef struct {
    uint64_t MINUTES;      // Bit n = minute n (0-59)
    uint32_t HOURS;        // Bit n = heure n (0-23)
    uint8_t DAYSOFWEEK;    // Bit n = jour n (0=dim, 6=sam)
} timing;

// Vérification  :
bool should_run(tasks *T, struct tm *tm_now) {
    return (T->tm.MINUTES & (1ULL << tm_now->tm_min)) &&
           (T->tm.HOURS & (1U << tm_now->tm_hour)) &&
           (T->tm.DAYSOFWEEK & (1 << tm_now->tm_wday));
}
```

**Concept appliqué** : Manipulations bit-à-bit.

---

## 6. Persistence sur disque

Concept appliqué : **Fichiers**, **Répertoires** 

### Lecture de l'arborescence

```c
tasks *read_tasks(char *chemin) {
    DIR *dir = opendir(chemin);              // Ouvrir répertoire
    struct dirent *entry;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0) continue;
        
        // entry->d_name est l'ID (ex: "1", "2"...)
        tasks tache;
        tache.ID = strtoull(entry->d_name, NULL, 10);
        
        // Lire les métadonnées : timing, commande...
        struct stat st;
        stat(chemin_timing, &st);
        
        // Construire la structure
    }
    closedir(dir);
    return TASKS;
}
```

**Appels système utilisés** :
- `opendir()` / `readdir()` / `closedir()` : parcours du répertoire
- `stat()` : consulter les i-nœuds
- `open()` / `read()` : accès aux fichiers de données 

### Sauvegarde

```c
int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
write_u64(fd, task.tm.MINUTES);    
write_u32(fd, task.tm.HOURS);
write(fd, &task.tm.DAYSOFWEEK, 1);
close(fd);
```

**Avantage** : Les données survivent à l'arrêt du démon. Au redémarrage, `read_tasks()` les charge.

---

## 7. Sérialisation big-endian

Concept appliqué : **Conversion endian** 

```c
// Écriture 
void write_u16(int fd, uint16_t v) {
    uint16_t be = htobe16(v);
    write(fd, &be, 2);
}

// Lecture 
uint16_t read_u16(int fd) {
    uint16_t v;
    read_all(fd, &v, 2);
    return be16toh(v);
}
```

**Raison** : Garantir que les données binaires s'échangent correctement entre le client et le serveur, indépendamment de l'architecture.

---

## 8. Points clés du design

### Choix 1 : Signaux + alarme 

**Choix fait** : Signaux + `alarm()` + `read()` bloquant

**Pourquoi ?**
- Pas de ressources gaspillées en boucle 
- `alarm()` réveille le démon à l'heure exacte 
- `read()` bloquant sur le pipe permet la synchronisation client-serveur
- Appel système interrompu par le signal revient avec EINTR 

### Choix 2 : Tubes nommés 

**Choix fait** : Tubes nommés

**Pourquoi ?**
- Respecte les attendus du projet.  

### Choix 3 : Double fork pour les tâches

**Choix fait** : Double fork

**Pourquoi ?**
- Éviter les zombies
- Le père n'a qu'à faire `wait()` sur le premier fils
- Le deuxième fils exécute vraiment, autonome

---


## Conclusion

Le projet est une **application directe** des primitives système du cours :
- **Processus** : fork + exec pour exécuter des commandes
- **Synchronisation** : signaux + tubes pour la communication
- **Persistence** : fichiers pour sauvegarder les tâches
- **Robustesse** : gestion propre des signaux, double fork, tubes nommés

