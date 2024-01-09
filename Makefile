all: server client

server: Server.cpp
	gcc -std=c++20 -pthread -o SEMESTRALKA_SERVER Server.cpp -L /usr/lib/x86_64-linux-gnu/

client: Client.cpp
	gcc -std=c++20 -pthread -o SEMESTRALKA_CLIENT Client.cpp

clean:
	rm -f SEMESTRALKA_SERVER SEMESTRALKA_CLIENT