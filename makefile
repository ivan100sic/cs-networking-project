CCC=g++ -pthread -O2 -std=c++14 -Wall
all: server client

server: server.cpp common.hpp
	$(CCC) server.cpp -o server

client: client.cpp common.hpp
	$(CCC) client.cpp -o client

streamserver: streamserver.cpp common.hpp
	$(CCC) streamserver.cpp -o streamserver

streamclient: streamclient.cpp common.hpp
	$(CCC) streamclient.cpp -o streamclient

stream: streamserver streamclient

soundtest: soundtest.cpp
	$(CCC) soundtest.cpp -o soundtest
