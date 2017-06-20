CCC=g++ -pthread -O2 -std=c++14 -Wall
all: server client streamserver streamclient stream soundtest sound soundserver soundclient

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
	$(CCC) soundtest.cpp -lasound -o soundtest

sound: soundserver soundclient

soundserver: soundserver.cpp sound.hpp common.hpp
	$(CCC) soundserver.cpp -lasound -o soundserver

soundclient: soundclient.cpp sound.hpp common.hpp
	$(CCC) soundclient.cpp -lasound -o soundclient