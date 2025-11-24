# Projet systèmes exploitations 2025

Pour compiler:

Full:
make OU gcc -Wall -Wextra -o erraid erraid.c scheduler.c lecteur.c executor.c logger.c

Effacer tous les fichiers compilés:
make distclean

Tests lecteur:
gcc -Wall -Wextra -o testeur testeur.c lecteur.c

Tester lecteur sur une arborescence:
./testeur -r \<chemin>

Lancer erraid:

Dossier /tmp/$USER/erraid :
./erraid 

Chemin tasks custom:
./erraid -r \<chemin>


