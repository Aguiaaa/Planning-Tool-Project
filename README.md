# Projet systèmes exploitations 2025

## Pour compiler:

**Full:**  
```
make
```  
OU  
```
<<<<<<< HEAD
gcc -Wall -Wextra -o erraid erraid.c scheduler.c lecteur.c executor.c logger.c
=======
gcc -Wall -Wextra -o erraid erraid.c task_runner.c
>>>>>>> boucle_execution
```

**Effacer tous les fichiers compilés:**  
```
make distclean
```

---

<<<<<<< HEAD
## Tests lecteur:

```
gcc -Wall -Wextra -o testeur testeur.c lecteur.c
```

**Tester lecteur sur une arborescence:**  
```
./testeur -r <chemin>
```

---

## Lancer erraid:

**Dossier /tmp/$USER/erraid :**  
```
./erraid
```

=======

## Lancer erraid:

**Dossier /tmp/$USER/erraid :**  
```
./erraid
```

>>>>>>> boucle_execution
**Chemin tasks custom:**  
```
./erraid -r <chemin>
```
