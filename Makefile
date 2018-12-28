all: server client
.PHONY: all

CC = gcc

server: server.o mftputil.o
	@$(CC) -o server server.o mftputil.o
client: client.o mftputil.o
	@$(CC) -o client client.o mftputil.o

server.o:
	@$(CC) -c server.c -o server.o

client.o:
	@$(CC) -c client.c -o client.o

mftputil.o:
	@$(CC) -c mftputil.c -o mftputil.o

.PHONY: clean
clean:
	@rm -f *.o server client
	@echo "cleaned"