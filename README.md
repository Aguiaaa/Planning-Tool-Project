# Projet outil de planification - 2025 
## erraid & tadmor — Planificateur de tâches

Ce projet implémente un système de planification de tâches inspiré de `cron`, composé :

- d'un **démon** `erraid`, chargé d'exécuter des tâches selon des horaires donnés ;
- d'un **client** `tadmor`, permettant d'envoyer des requêtes au démon (création, suppression, consultation de tâches).

Le système supporte :
- Les **tâches simples** (exécution d'une commande unique)
- Les **combinaisons de tâches** : séquences (`;`), pipelines (`|`), conditionnelles (`if-then-else`)
- La **persistence** des données sur disque (arborescence de stockage)
- L'**historique d'exécution** (timestamps et codes de retour)
- La **capture** de stdout/stderr pour chaque exécution

---

## Compilation

Compiler les exécutables `erraid` et `tadmor` :
```bash
make
```

Nettoyer les fichiers compilés :
```bash
make distclean
```

---

## Architecture du stockage

L'arborescence de stockage par défaut est `/tmp/$USER/erraid/` et contient :

```
/tmp/$USER/erraid/
├── pipes/                    # Répertoire des tubes nommés
│   ├── erraid-request-pipe   # Pipe: requêtes client → démon
│   └── erraid-reply-pipe     # Pipe: réponses démon → client
└── tasks/                    # Répertoire des tâches
    ├── 1/
    │   ├── timing            # Horaire d'exécution
    │   ├── cmd/              # Définition de la commande
    │   │   ├── type
    │   │   └── argv (ou sous-répertoires pour combinaisons)
    │   ├── stdout            # Sortie standard de la dernière exécution
    │   ├── stderr            # Erreur standard de la dernière exécution
    │   └── times-exitcodes   # Historique des exécutions
    └── 2/
        └── ...
```

---

## Lancement du démon `erraid`

### Utilisation simple :
```bash
./erraid
```
Démarre le démon en tâche de fond avec les répertoires par défaut.

### Options disponibles

```bash
./erraid [-R RUN_DIR] [-P PIPES_DIR] [-F]
```

| Option | Description |
|--------|-------------|
| `-R RUN_DIR` | Répertoire de stockage (défaut: `/tmp/$USER/erraid`) |
| `-P PIPES_DIR` | Répertoire des pipes (défaut: `RUN_DIR/pipes`) |
| `-F` | Exécution en avant-plan (pas de démonisation) |

### Exemples :

```bash
# Lancer avec stockage personnalisé
./erraid -R /home/user/my_tasks

# Lancer en avant-plan pour déboguer
./erraid -F

# Combiner options
./erraid -R /tmp/mytasks -P /tmp/mytasks/pipes
```

---

## Utilisation du client `tadmor`

`tadmor` envoie des requêtes au démon et affiche les réponses.

### Options de consultation

#### `-l` : Lister les tâches
```bash
tadmor -l
```

Affiche les tâches avec : ID, horaires (format cron), commande

Exemple de sortie :
```
1: 30 19 1 echo "Bonsoir"
2: 0 5 * date
3: - - - echo "Tâche abstraite"
```

#### `-x TASKID` : Historique d'exécution
```bash
tadmor -x 1
```

Affiche la liste datée des exécutions avec codes de retour :
```
2025-01-01 19:30:00 0
2025-01-02 19:30:00 0
```

#### `-o TASKID` : Sortie standard
```bash
tadmor -o 1
```

Affiche la stdout de la dernière exécution.

#### `-e TASKID` : Sortie erreur
```bash
tadmor -e 1
```

Affiche la stderr de la dernière exécution.

---

### Options de création

#### `-c` : Créer une tâche simple
```bash
tadmor -c [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] CMD [ARG1] [ARG2] ...
```

Crée une tâche exécutant une commande simple.

| Option | Description |
|--------|-------------|
| `-m MINUTES` | Minutes d'exécution (ex: `0,30` ou `*/15`) |
| `-H HOURS` | Heures d'exécution (ex: `9-17`) |
| `-d DAYSOFWEEK` | Jours (0=dim, 6=sam; ex: `1-5` pour lun-ven) |
| `-n` | Tâche abstraite (sans horaire, pour combinaison future) |

**Exemples :**
```bash
# Exécuter "date" tous les jours à 5h
tadmor -c -H 5 date

# Exécuter "backup.sh" lun-ven à 9h30
tadmor -c -m 30 -H 9 -d 1-5 ./backup.sh

# Créer une tâche abstraite (sans horaire)
tadmor -c -n echo "Hello"
```

#### `-s` : Séquence de tâches (`;`)
```bash
tadmor -s [-m MIN] [-H HRS] [-d DAYS] TASKID1 TASKID2 ...
```

Crée une nouvelle tâche combinant plusieurs tâches en séquence.

```bash
tadmor -s 1 2 3  # Exécute: cmd1 ; cmd2 ; cmd3
```

#### `-p` : Pipeline de tâches (`|`)
```bash
tadmor -p [-m MIN] [-H HRS] [-d DAYS] TASKID1 TASKID2 ...
```

Crée une nouvelle tâche combinant plusieurs tâches en pipeline.

```bash
tadmor -p 1 2  # Exécute: cmd1 | cmd2
```

#### `-i` : Combinaison conditionnelle (`if-then-else`)
```bash
tadmor -i [-m MIN] [-H HRS] [-d DAYS] TASKID1 TASKID2 [TASKID3]
```

Crée une tâche conditionnelle.

```bash
tadmor -i 1 2 3  # Exécute: if cmd1 ; then cmd2 ; else cmd3 ; fi
tadmor -i 1 2    # Exécute: if cmd1 ; then cmd2 ; fi
```

---

### Options de gestion

#### `-r TASKID` : Supprimer une tâche
```bash
tadmor -r 1
```

Supprime la tâche et tous ses fichiers de log.

#### `-q` : Arrêter le démon
```bash
tadmor -q
```

Termine proprement le démon.

#### `-P PIPES_DIR` : Répertoire des pipes personnalisé
```bash
tadmor -P /tmp/custom/pipes -l
```

Utilise un répertoire de pipes différent de la valeur par défaut.

---

## Format des horaires (cron)

Les horaires utilisent le même format que `crontab` :

| Champ | Plage | Exemple | Signification |
|-------|-------|---------|---------------|
| Minutes | 0-59 | `0,30` | À 0 et 30 minutes |
| Hours | 0-23 | `9-17` | De 9h à 17h |
| Days | 0-6 (0=dim) | `1-5` | Lundi à vendredi |

**Syntaxes acceptées :**
- `*` : tous les moments
- `3` : moment précis
- `3,5,7` : énumération
- `3-7` : plage
- Combinaisons : `0,15,30,45` ou `1-5,10`

---

## Exemple d'utilisation



```bash
# Créer deux tâches
tadmor -c -H 9 echo "Bonjour"        # ID 1
tadmor -c -m 0 echo "Toutes heures"  # ID 2

# Lister
tadmor -l
# Résultat:
# 1: * 9 * echo Bonjour
# 2: 0 * * echo Toutes heures

# Consulter l'historique
tadmor -x 1

# Voir la sortie
tadmor -o 1

# Supprimer
tadmor -r 1
```
