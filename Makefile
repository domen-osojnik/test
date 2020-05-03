all: ociscevalnik helloworld
ociscevalnik: main.c
	gcc -o ociscevalnik main.c

helloworld: helloworld.c
	gcc -o helloworld helloworld.c
