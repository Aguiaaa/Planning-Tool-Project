# Projet outil de planification - 2025 
## erraid & tadmor — Jalon 2

Ce projet implémente un système de planification de tâches inspiré de `cron`, composé :

- d’un **démon** `erraid`, chargé d’exécuter des tâches selon des horaires donnés ;
- d’un **client** `tadmor`, permettant d’envoyer des requêtes consultatives au démon.

Pour ce jalon 2, le démon doit exploiter une arborescence statique de tâches (simples ou séquentielles) et répondre aux requêtes du client.

---

## Compilation

Compiler les exécutables `erraid` et `tadmor` :
```
make
```

ou manuellement :
```
gcc -Wall -Wextra -o erraid erraid.c task_runner.c protocole.c parsing_tasks.c
gcc -Wall -Wextra -o tadmor tadmor.c protocole.c
```

Nettoyer les fichiers compilés :
```
make distclean
```

---

## Lancement du démon `erraid`

Utilisation avec le répertoire par défaut :  
```
./erraid
```

Spécifier un répertoire de stockage (contenant `tasks/`) :
```
./erraid -r <chemin>
```
Spécifier le répertoire contenant les tubes de communication (par defaut `/tmp/$USER/erraid/pipes`) :
```
./erraid -p <chemin>
```

À son démarrage, `erraid` crée les pipes nommés nécessaires :  
- `erraid-request-pipe`  
- `erraid-reply-pipe`

---

## Utilisation du client `tadmor`

`tadmor` envoie une requête au démon via le pipe de requête  
et affiche la réponse reçue via le pipe de réponse.

### Options implémentées (Jalon 2)

#### Requêtes consultatives
```
-l
```
Affiche la liste des tâches définies dans l’arborescence.

```
-x TASKID
```
Affiche la liste datée des valeurs de retour de la tâche `TASKID`.

```
-o TASKID
```
Affiche la sortie standard de la dernière exécution complète.

```
-e TASKID
```
Affiche l’erreur standard de la dernière exécution complète.

#### Spécifier le répertoire des pipes
```
-p <chemin_pipes>
```
Permet d’utiliser un répertoire pipes différent de la valeur par défaut.

---

## Exemples d’utilisation

Liste des tâches :
```
tadmor -l
```

Voir les retours d’exécution :
```
tadmor -x 42
```

Sortie standard de la dernière exécution :
```
tadmor -o 42
```

Erreur standard :
```
tadmor -e 42
```

Utilisation avec un répertoire pipes personnalisé :
```
tadmor -p /tmp/$USER/erraid/pipes -l
```

---

## Notes

- Les fonctionnalités de création/suppression de tâches (`-c`, `-s`, `-r`, etc.) seront implémentées au **rendu final**.  

