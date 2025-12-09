# Architecture du Projet Erraid & Tadmor

## 1. Vue d'ensemble

Le projet est constitué de deux programmes distincts communiquant entre eux via des tubes nommés (FIFO), suivant le modèle **Client-Serveur** local :
* **`erraid` (le démon)** : Charge de planifier et d'exécuter les tâches. Il tourne en tâche de fond.
* **`tadmor` (le client)** : Interface utilisateur pour envoyer des requêtes au démon.

Ce choix d'architecture repose sur l'utilisation des **tubes nommés** (`mkfifo`) pour permettre la communication entre processus non apparentés, comme vu dans le **Cours 10 (p. 7-11)**.

---

## 2. Structures de Données

Les données sont structurées pour refléter l'arborescence de fichiers décrite dans le sujet. Les structures principales sont définies dans `erraid.h` :

* **`tasks`** : Représente une tâche complète (ID, contraintes horaires `timing`, commande racine).
* **`command`** : Structure récursive (arbre) permettant de gérer les commandes complexes (séquences).

---

## 3. Fonctionnement du Démon (`erraid`)

Le démon implémente une boucle d'événements basée sur les signaux et les interruptions d'appels système.

### 3.1. Initialisation
Au démarrage, `erraid` :
1.  Parse l'arborescence des tâches (`parsing_tasks.c`).
2.  Crée les tubes nommés (requête et réponse) avec `mkfifo`.
3.  Configure les gestionnaires de signaux via **`sigaction`** (plus fiable que `signal`, cf. **Cours 12 p. 3**) :
    * `SIGALRM` : Pour la planification périodique.
    * `SIGINT`/`SIGTERM` : Pour un arrêt propre.
    * `SIGPIPE` : Ignoré pour éviter la terminaison brutale en cas de déconnexion client (**Cours 10 p. 3**).

### 3.2. Boucle Principale et Multiplexage
Le démon doit attendre simultanément une échéance temporelle (minute suivante) et des requêtes client.
* Nous utilisons un appel système `read()` bloquant sur le tube de requête.
* Nous armons une alarme (`alarm()`).
* Si l'alarme sonne, `read()` est interrompu et retourne l'erreur **`EINTR`** (**Cours 12 p. 7**). Le démon intercepte cette interruption, lance les tâches, puis reprend son attente.

**Choix technique (Tube bloquant) :** Le tube de requête est ouvert en mode `O_RDWR`. Cela permet de garder un écrivain potentiel en permanence et d'éviter que `read` ne renvoie `0` (EOF) en boucle, une astuce mentionnée pour Linux dans le **Cours 10 (p. 9)**.

### 3.3. Exécution des Tâches (`task_runner.c`)
L'exécution utilise une approche récursive :
* **Création de processus :** Utilisation de `fork()` et `execvp()` (**Cours 7 p. 5**).
* **Redirections :** Les sorties `stdout` et `stderr` sont redirigées vers les fichiers de logs via `dup2()` avant le recouvrement (**Cours 3 p. 10**).
* **Gestion des Zombies (Double Fork) :** Pour éviter de gérer les signaux `SIGCHLD` ou de bloquer sur `wait`, nous utilisons la technique du **double fork** : le démon crée un fils, qui crée un petit-fils pour la commande puis termine immédiatement. Le petit-fils est alors adopté par `init`. Cette stratégie est explicitement recommandée pour les démons dans le **Cours 7 (p. 18)**.
* **Nettoyage :** Nous appliquons la "règle d'or" consistant à fermer les descripteurs hérités inutiles (comme le tube de requête) dans les processus fils (**Cours 9 p. 9**).

---

## 4. Fonctionnement du Client (`tadmor`)

Le client suit un flux séquentiel :
1.  Ouverture du tube requête (`O_WRONLY`).
2.  Envoi de la requête formatée (Big Endian).
3.  Ouverture du tube réponse (`O_RDONLY`). Note : cette ouverture bloque tant que le démon n'ouvre pas le tube en écriture, assurant une synchronisation (**Cours 10 p. 9**).
4.  Lecture et affichage de la réponse.

---

## 5. Gestion des Erreurs et Robustesse

* **Interruptions :** La fonction utilitaire `read_all` gère explicitement le cas `errno == EINTR` pour relancer la lecture si elle est interrompue par un signal, garantissant la fiabilité des échanges (**Cours 12 p. 7**).
* **Variable globale :** L'arrêt de la boucle principale se fait via une variable de type `volatile sig_atomic_t` modifiée par le handler, assurant l'atomicité de l'opération (**Cours 12 p. 6**).
* **Nettoyage :** En fin d'exécution, le démon supprime les tubes nommés avec `unlink` pour laisser le système propre (**Cours 6 p. 7**).