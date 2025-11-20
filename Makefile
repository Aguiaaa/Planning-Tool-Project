all: erraid tadmor

erraid: erraid.c task_runner.o
	gcc -Wall -Wextra -O2 -o erraid erraid.c task_runner.o

tadmor: tadmor.c
	gcc -Wall -Wextra -O2 -o tadmor tadmor.c

task_runner.o: task_runner.c
	gcc -Wall -Wextra -O2 -c task_runner.c

distclean:
	rm -f erraid tadmor task_runner.o
