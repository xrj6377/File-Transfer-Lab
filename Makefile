CC	 	= gcc
FLAGS	= -O2

all:
	$(CC) $(FLAGS) server/server.c -o server/server
	$(CC) $(FLAGS) client/deliver.c -o client/deliver
server:
	$(CC) $(FLAGS) server/server.c -o server/server
deliver:
	$(CC) $(FLAGS) client/deliver.c -o client/deliver
clean:
	rm -rf server/server client/deliver