# Projet Système : Planificateur de tâches (Erraid & Tadmor)

Projet réalisé dans le cadre du cours de Système d'Exploitation.
L'objectif est de créer un outil capable de programmer l'exécution de commandes à l'avance, un peu comme le `cron` de Linux.


## Description
Le projet se compose de deux programmes :
1. **erraid** : Le démon (serveur). Il tourne en arrière-plan, surveille l'heure et lance les tâches.
2. **tadmor** : Le client. Il permet d'envoyer des commandes au démon.

## Compilation
```bash
make            # Crée les exécutables erraid et tadmor
make distclean  # Supprime les fichiers compilés
```

## Manuel d'utilisation

### 1. Le Démon (erraid)

**Lancement standard :**
```bash
./erraid
```
Le démon se détache et tourne en arrière-plan.

**Options disponibles :**
- `-R <dir>` : Spécifie le dossier de stockage (par défaut `/tmp/$USER/erraid/tasks`).
- `-P <dir>` : Spécifie le dossier des tubes nommés (par défaut `/tmp/$USER/erraid/pipes`).
- `-F` : Force l'exécution en avant-plan (pas de mode démon), utile pour le débogage.

*Exemple complet :*
```bash
./erraid -F -R ./mes_taches -P ./mes_pipes
```

### 2. Le Client (tadmor)

Toutes les commandes acceptent l'option `-P <dir>` pour spécifier où chercher les tubes du démon.

#### A. Consultation et Gestion
- **Lister les tâches :**
  ```bash
  ./tadmor -l
  ```
- **Supprimer une tâche :**
  ```bash
  ./tadmor -r <ID>
  ```
- **Arrêter le démon :**
  ```bash
  ./tadmor -q
  ```

#### B. Inspection des résultats
- **Historique des exécutions** (dates et codes de retour) :
  ```bash
  ./tadmor -x <ID>
  ```
- **Voir la sortie standard (stdout) de la dernière exécution :**
  ```bash
  ./tadmor -o <ID>
  ```
- **Voir la sortie d'erreur (stderr) de la dernière exécution :**
  ```bash
  ./tadmor -e <ID>
  ```

#### C. Création de tâches simples
Créer une tâche nécessite `-c`.
Les horaires se définissent avec :
- `-m` : Minutes (0-59)
- `-H` : Heures (0-23)
- `-d` : Jours (0=Dimanche, 6=Samedi)
- `-n` : Tâche abstraite (sans horaire, ne s'exécute jamais seule)



#### D. Combinaisons de tâches
Vous pouvez combiner des tâches existantes (par leurs IDs).

- **Séquence (`;`)** - Option `-s` :
  Exécute les tâches l'une après l'autre.
  ```bash
  ./tadmor -s <ID1> <ID2>
  ```

- **Pipeline (`|`)** - Option `-p` :
  La sortie de la tâche 1 devient l'entrée de la tâche 2.
  ```bash
  ./tadmor -p <ID1> <ID2>
  ```

- **Conditionnelle (`if/then/else`)** - Option `-i` :
  Si la tâche 1 réussit (code 0), lance la tâche 2, sinon la 3.
  ```bash
  ./tadmor -i <ID_COND> <ID_THEN> <ID_ELSE>
  ```

## Structure du projet
- `erraid.c` : Boucle principale du démon, gestion des signaux et initialisation.
- `tadmor.c` : Parsing des arguments ligne de commande et envoi des requêtes.
- `task_runner.c` : Logique d'exécution (fork/exec, redirection flux, vérification horaires).
- `parsing_tasks.c` : Gestion de la persistance (lecture/écriture récursive sur disque).
- `protocole.c` : Sérialisation binaire (Big-Endian) pour la communication via pipes.
