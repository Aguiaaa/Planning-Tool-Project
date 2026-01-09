# Architecture du projet Erraid & Tadmor

## Vue d'ensemble
Nous avons découpé le projet en deux exécutables distincts pour séparer le démon (serveur) et l'interface utilisateur (client) :
1. **erraid** : Le démon qui tourne en fond, gère le stockage et exécute les tâches.
2. **tadmor** : Le client qui permet d'envoyer des commandes au démon via des tubes nommés.

## Structures de données
Pour gérer les tâches en mémoire, nous avons défini plusieurs structures dans `erraid.h` :

- **`tasks`** : C'est la structure principale. Elle contient l'ID de la tâche, son timing, et un pointeur vers la commande à exécuter.
- **`timing`** : Pour stocker les horaires (minutes, heures, jours), nous utilisons des entiers (`uint64`, `uint32`) comme des masques de bits. Cela permet de vérifier rapidement si l'heure correspond.
- **`command`** : Comme une commande peut être complexe (séquence, pipe...), nous avons utilisé une structure récursive (un arbre). Une commande contient soit des arguments (si elle est simple), soit une liste de sous-commandes.

## Algorithmes implémentés

### 1. Lecture des tâches (Parsing)
Pour charger les tâches depuis le dossier de sauvegarde, nous utilisons une fonction récursive (`read_cmd` dans `parsing_tasks.c`).
- On parcourt les dossiers.
- Si on trouve un fichier `argv`, c'est une commande simple : on lit les arguments.
- Si on trouve des sous-dossiers (0, 1, 2...), c'est une commande complexe : la fonction s'appelle elle-même pour charger chaque sous-commande.

### 2. Vérification des horaires
Dans `task_runner.c`, la fonction `should_run` vérifie si une tâche doit se lancer maintenant.
On utilise des opérations bit-à-bit (`&`) entre le masque de la tâche et l'heure actuelle. Par exemple, pour les minutes, on vérifie si le bit correspondant à la minute actuelle est à 1.

### 3. Exécution des commandes
L'exécution (`run` dans `task_runner.c`) suit la structure de l'arbre de commande :
- **Commande simple** : On fait un `fork` puis un `execvp`.
- **Séquence (;)** : On exécute les commandes les unes après les autres.
- **Pipeline (|)** : On crée des pipes dans une boucle. On utilise `dup2` pour brancher la sortie standard d'une commande sur l'entrée standard de la suivante.
- **Condition (if)** : On lance la première commande. Si elle réussit (retour 0), on lance la suite ("then"), sinon on lance le "else".

### 4. Fonctionnement du Démon
Le démon (`erraid.c`) fonctionne avec une boucle principale :
1. On calcule le temps restant avant la prochaine minute et on met une `alarm`.
2. On se met en attente de lecture (`read`) sur le pipe de requêtes pour écouter `tadmor`.
3. Si l'alarme sonne (`SIGALRM`), le `read` est interrompu. On parcourt alors la liste des tâches et on lance celles qui doivent s'exécuter via un double `fork` (pour éviter les processus zombies).
4. Si on reçoit une requête du client, on la traite (création, suppression, etc.).

## Gestion de la mémoire
Comme le démon doit tourner longtemps, nous avons fait attention aux fuites de mémoire. Lors du rechargement des tâches (après une création ou suppression), nous appelons `free_tasks` qui parcourt récursivement toute la structure pour tout libérer proprement avant de recharger.