
all: erraid tadmor

erraid: erraid.c
	 gcc -Wall -Wextra -o erraid erraid.c scheduler.c lecteur.c executor.c logger.c

tadmor: tadmor.c
	gcc -Wall -Wextra -O2 -o tadmor tadmor.c

distclean:
	rm -f erraid tadmor
