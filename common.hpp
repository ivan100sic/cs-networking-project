#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string>
#include <cstring>
#include <thread>
using namespace std;

typedef struct sockaddr_in sockai;
typedef struct PACKET packet_t;
typedef struct sockaddr socka;

// Odrzava jedan socket
struct Socket {

	int sock_id;

	explicit Socket(int sock_id) : sock_id(sock_id) {}

	Socket() : sock_id(-1) {}

	void send(const string& data) {
		if (sock_id == -1) return;
		const char* p = data.c_str();
		size_t sent = 0;
		while (sent < data.size()) {
			ssize_t status = ::send(sock_id, p+sent, data.size()-sent, 0);
			if (status <= 0) {
				// error handling
				return;
			}
			sent += status;
		}
	}

	void finish() {
		if (sock_id == -1) return;
		::send(sock_id, nullptr, 0, MSG_EOR);
	}

	Socket& operator= (Socket&& other) {
		tidy();
		sock_id = other.sock_id;
		other.sock_id = -1;
	}

	string receive(int buf_size = 4096) {
		if (sock_id == -1) return "";
		char* buffer = new char[buf_size];
		string result;
		while (1) {
			ssize_t status = recv(sock_id, buffer, buf_size, 0);
			if (status <= 0) {
				delete[] buffer;
				return result;
			} else {
				result += string(buffer, buffer+status);
			}
		}
	}

	void tidy() {
		if (sock_id != -1) {
			shutdown(sock_id, 0);
		}
	}

	~Socket() {
		tidy();
	}

	static int get_client(const string& address, int port) {
		sockai a;
		memset(&a, 0, sizeof(a));
		a.sin_family = AF_INET;
		a.sin_addr.s_addr = inet_addr(address.c_str());
		a.sin_port = htons(port);
		
		int sock_id = socket(AF_INET, SOCK_STREAM, 0);

		int en = 1;
    	setsockopt(sock_id, SOL_SOCKET, SO_REUSEADDR, &en, sizeof(int));

		int conn = connect(sock_id, (socka*)&a, sizeof(a));
#ifdef DEBUG
		cerr << "Socket ID: " << sock_id << '\n';
		cerr << "Connection ID: " << conn << '\n';
#endif
		return sock_id;
	}

	static int get_server(int port, int backlog_size = 10) {
		sockai a;
		memset(&a, 0, sizeof(a));
		a.sin_family = AF_INET;
		a.sin_port = htons(port);

		int sock_id = socket(AF_INET, SOCK_STREAM, 0);
		
		int en = 1;
		setsockopt(sock_id, SOL_SOCKET, SO_REUSEADDR, &en, sizeof(int));
		
		bind(sock_id, (socka*)&a, sizeof(a));
		listen(sock_id, backlog_size);
 
#ifdef DEBUG
		cerr << "Socket ID: " << sock_id << '\n';
#endif
		return sock_id;
	}

	void finish_sending() {
		shutdown(sock_id, SHUT_WR);
	}
};

// Odrzava jednu klijentsku konekciju
struct Connection : public Socket {

	Connection(const string& address, int port = 80) :
		Socket(Socket::get_client(address, port)) {}
};

// Prima podatke sa socketa, zove funkciju, salje odgovor
// i zatvara socket. 
template<class T>
void serve(int sock_id, T cb) {
	Socket sock(sock_id);
	string req = sock.receive();
#ifdef DEBUG
	cerr << "Received request of size " << req.size() << '\n';
#endif
	string resp = cb(req);
#ifdef DEBUG
	cerr << "Sending response of size " << resp.size() << '\n';
#endif
	sock.send(resp);
	sock.finish_sending();
}

// Slusa za zahteve na datom portu i odgovara na upite.
// Moguce su paralelne konekcije
struct SimpleParallelServer {

	int port;

	SimpleParallelServer(int port) : port(port) {}

	template<class T>
	void run(T cb) {
		int server_sock = Socket::get_server(port);
		while (1) {
			sockai remote;
			socklen_t len = sizeof(sockai);
			// int sock = accept(server_sock, (socka*)&remote, &len);
			int sock = accept(server_sock, nullptr, nullptr);
			thread t(serve<T>, sock, cb);
			t.detach();
		}
	}
};