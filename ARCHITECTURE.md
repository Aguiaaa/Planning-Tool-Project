# Architecture du projet Erraid & Tadmor

## Vue globale
On a séparé le projet en deux programmes différents pour bien distinguer le client et le serveur :
1. **erraid** : C'est le démon (le serveur). Il tourne tout le temps en arrière-plan, garde les tâches en mémoire et s'occupe de les lancer à la bonne heure.
2. **tadmor** : C'est le client. C'est juste une commande qui permet à l'utilisateur d'envoyer des ordres au démon (comme "crée une tâche" ou "liste les tâches") via des tubes nommés.

## Comment on stocke les données (`erraid.h`)
Pour gérer les tâches dans le démon, on utilise plusieurs structures :

- **`tasks`** : La structure de base. Elle contient l'ID, le timing, et la commande à lancer.
- **`timing`** : On utilise des entiers pour stocker les minutes, heures et jours sous forme de bits. Ça nous permet de vérifier très vite si une tâche doit se lancer (avec des opérations `&`).
- **`command`** : Comme une commande peut être compliquée (des pipes, des séquences...), on a fait une structure récursive (un arbre).
    - Soit c'est une commande simple (avec des arguments).
    - Soit c'est une combinaison (Séquence, Pipe, If) qui contient d'autres commandes en dessous.

## Nos Algorithmes

### 1. Charger les tâches (Parsing)
Pour lire les tâches depuis le disque, on a une fonction récursive (`read_cmd`) qui se promène dans les dossiers :
- Elle regarde le fichier `type` pour savoir ce que c'est.
- Si c'est une commande complexe, elle descend dans les sous-dossiers `0`, `1`... pour tout reconstruire en mémoire.

### 2. Vérifier l'heure
Dans `task_runner.c`, la fonction `should_run` regarde si une tâche doit se lancer. Elle compare simplement l'heure actuelle avec les masques de bits de la tâche. Si ça correspond partout (minute, heure, jour), on lance !

### 3. Exécuter les commandes
La fonction `run` parcourt l'arbre de la commande :
- **Simple** : On fait un `fork` et un `execvp`.
- **Pipe (|)** : On crée des tubes (`pipe`) et on utilise `dup2` pour relier la sortie d'une commande à l'entrée de la suivante.
- **If** : On lance la première commande, on attend son résultat, et selon si elle a réussi ou pas, on lance la suite (then ou else).

## Fonctionnement du Démon 
Au début, on utilisait `alarm` et des signaux pour réveiller le démon chaque minute. C'était compliqué et ça posait des problèmes avec les appels systèmes interrompus (`EINTR`).

On a décidé de tout changer pour utiliser **`poll`**. Voici comment ça marche maintenant dans le `main` :

1. **On calcule l'attente** : On regarde combien de millisecondes il reste avant la prochaine minute pile.
2. **On s'endort** : On appelle `poll`. Le démon ne fait rien et ne consomme pas de CPU.
3. **Le réveil** : Il se réveille seulement si :
    - Le temps est écoulé -> On vérifie les tâches et on les lance.
    - On reçoit une commande sur le tube -> On la traite tout de suite.

Pour lancer les tâches, on utilise la technique du **"Double Fork"**. Le démon crée un fils, qui crée un autre fils (la tâche) et meurt tout de suite. Comme ça, la tâche est adoptée par le système (init) et on n'a pas de processus zombies qui traînent.

## Nos Optimisations 

- **`rename` au lieu de copier** : Quand on combine des tâches (par exemple pour faire un Pipe), au lieu de copier tous les fichiers un par un (ce qui est long et risqué), on utilise `rename`. Ça déplace juste le dossier instantanément. C'est "atomique", donc plus sûr.
- **Nettoyage avec `kill`** : Quand on arrête le démon, on veut être sûr que toutes les tâches s'arrêtent aussi. On utilise `kill(0, SIGTERM)` qui envoie le signal d'arrêt à tout notre groupe de processus. Comme ça, il ne reste rien qui tourne en arrière-plan.