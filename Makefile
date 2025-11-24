all: erraid tadmor

<<<<<<< HEAD
erraid: erraid.c
	 gcc -Wall -Wextra -o erraid erraid.c scheduler.c lecteur.c executor.c logger.c
=======
erraid: erraid.c 
	gcc -Wall -Wextra -o erraid erraid.c task_runner.c
>>>>>>> boucle_execution

tadmor: tadmor.c
	gcc -Wall -Wextra -o tadmor tadmor.c

distclean:
	rm -f erraid tadmor 
