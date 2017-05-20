all: server client

server: server.cpp common.hpp
	g++ -pthread -O2 -std=c++14 server.cpp -o server

client: client.cpp common.hpp
	g++ -pthread -O2 -std=c++14 client.cpp -o client

streamserver: streamserver.cpp
	g++ -pthread -O2 -std=c++14 streamserver.cpp -o streamserver

streamclient: streamclient.cpp
	g++ -pthread -O2 -std=c++14 streamclient.cpp -o streamclient

stream: streamserver streamclient

bane: bane-sserver

bane-sserver: bane-sserver.cpp
	g++ -pthread -O2 -std=c++14 bane-sserver.cpp -o bane-sserver
