# Architecture du Projet Erraid & Tadmor

## 1. Vue d'ensemble

Le projet implémente une architecture **Client-Serveur** locale pour la planification de tâches.
La communication repose sur l'utilisation de **tubes nommés (FIFO)**, car ils sont « de même nature que les tubes anonymes, mais avec une existence dans le SGF » et sont « accessibles par des processus non nécessairement apparentés ».

* **`erraid` (le démon)** : Serveur persistant qui gère l'exécution planifiée.
* **`tadmor` (le client)** : Interface utilisateur ponctuelle.

---

## 2. Structures de Données

Les données sont structurées pour refléter l'arborescence décrite dans le sujet. Les structures principales (`tasks` et `command` dans `erraid.h`) utilisent une définition récursive pour gérer les séquences de commandes complexes.

---

## 3. Fonctionnement du Démon (`erraid`)

Le démon implémente une boucle principale basée sur les signaux et les interruptions d'appels système.

### 3.1. Gestion des Signaux
La configuration des signaux utilise la primitive **`sigaction`** plutôt que `signal()`, car cette dernière est considérée comme une « primitive obsolète et absolument pas fiable ».

* **`SIGALRM`** : Utilisé pour la planification.
* **`SIGPIPE`** : Ignoré, car « une tentative d'écriture dans un tube sans lecteur provoque l'envoi de SIGPIPE », ce qui tuerait le démon si un client se déconnectait.

### 3.2. Gestion de l'Attente (Timer et Requêtes)
Le démon doit attendre soit une requête du client, soit que le temps s'écoule (nouvelle minute).
Nous utilisons un appel système `read()` bloquant. Lorsqu'une alarme survient, « un appel système (bloquant) interrompu par la réception d'un signal capté ne reprend pas: il retourne -1 et errno=EINTR ».
Notre boucle principale utilise ce mécanisme pour se réveiller à chaque minute (via l'erreur `EINTR`) tout en restant en attente de données sur le tube.

**Note sur l'ouverture :** Le tube est ouvert en `O_RDWR` pour éviter que `read` ne retourne 0 s'il n'y a aucun écrivain, une astuce mentionnée dans le cours pour Linux où « une ouverture en O_RDWR est possible » pour manipuler le comportement bloquant.

### 3.3. Exécution des Tâches (`task_runner.c`)
L'exécution exploite la séparation des étapes de création de processus sous UNIX :
1.  **Clonage (`fork`)** : Création d'un processus dont « l'espace d'adressage [...] est (initialement) une copie de celui du père ».
2.  **Redirections** : Entre le clonage et le recouvrement, nous modifions la table des descripteurs (avec `dup2`) pour rediriger `stdout` et `stderr`, car cela « laisse une opportunité pour modifier certaines choses impérativement parmi celles qui ne seront pas écrasées par le recouvrement ».
3.  **Recouvrement (`execvp`)** : « Remplacement de toute la mémoire par un nouveau segment de code ».

**Stratégie du "Double Fork" :**
Pour éviter les zombies sans bloquer le démon, nous utilisons le **double fork**, défini dans le cours comme une « stratégie pour ne pas avoir à attendre le fils [...] créer un fils puis un petit-fils [...] tuer le fils : le petit-fils est alors adopté par le processus 1 ».

**Gestion des ressources :**
Nous appliquons scrupuleusement la « Règle d'or: toujours libérer les descripteurs inutiles » en fermant les tubes hérités dans les processus fils.

---

## 4. Fonctionnement du Client (`tadmor`)

Le client utilise une synchronisation naturelle à l'ouverture : comme « une ouverture en écriture bloque [...] tant qu'il n'y a pas de lecteur », le client attend que le démon soit prêt avant d'envoyer sa requête.

Le protocole utilise le format **Big Endian** pour les entiers, assurant la portabilité des échanges binaires.

---

## 5. Robustesse

* **Sécurité des signaux :** Le gestionnaire de signal (`gestion_arret`) se limite à « la modification d'une variable globale dédiée de type volatile sig_atomic_t », respectant les contraintes de sécurité.
* **Nettoyage :** Le démon supprime les tubes nommés avec `unlink` à sa terminaison.