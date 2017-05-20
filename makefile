all: server client

server: server.cpp common.hpp
	g++ -pthread -O2 -std=c++14 server.cpp -o server

client: client.cpp common.hpp
	g++ -pthread -O2 -std=c++14 client.cpp -o client