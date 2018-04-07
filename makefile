build:
	gcc -Wall -Wpedantic -std=c99 ant_server.c -o ant_server 
run:
	./ant_server 8080
