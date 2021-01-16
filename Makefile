# NAME: Khoa Quach
# EMAIL: khoaquachschool@gmail.com
# ID: 105123806
CC = gcc
CFLAGS = -Wall -Wextra
default:
	$(CC) $(CFLAGS) -lz -o lab1b-client lab1b-client.c
	$(CC) $(CFLAGS) -lz -o lab1b-server lab1b-server.c
client:
	$(CC) $(CFLAGS) -lz -o lab1b-client lab1b-client.c

server: 
	$(CC) $(CFLAGS) -lz -o lab1b-server lab1b-server.c

dist: 
	tar -cvzf lab1b-105123806.tar.gz lab1b-server.c lab1b-client.c README Makefile

clean: 
	rm -f lab1b-105123806.tar.gz lab1b-client lab1b-server
