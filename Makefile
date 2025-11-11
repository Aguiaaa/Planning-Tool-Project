
all: erraid tadmor

erraid: erraid.c
	 gcc -Wall -Wextra -O2 -o erraid erraid.c

tadmor: tadmor.c
	gcc -Wall -Wextra -O2 -o tadmor tadmor.c

distclean:
	rm -f erraid tadmor
