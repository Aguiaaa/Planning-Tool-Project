# Projet systèmes exploitations 2025

Pour compiler:

Full:
gcc -Wall -Wextra -o erraid erraid.c scheduler.c lecteur.c executor.c logger.c

Tests lecteur:
gcc -Wall -Wextra -o testeur testeur.c lecteur.c

Tester lecteur sur une arborescence:
./testeur -r \<chemin>

Lancer erraid:

Dossier courant:
./erraid -r ./erraid_test/tasks

Chemin tasks custom:
./erraid -r \<chemin>


