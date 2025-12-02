all: erraid tadmor

erraid: erraid.c 
	gcc -Wall -Wextra -o erraid erraid.c task_runner.c protocole.c parsing_tasks.c

tadmor: tadmor.c
	gcc -Wall -Wextra -o tadmor tadmor.c protocole.c

distclean:
	rm -f erraid tadmor 
