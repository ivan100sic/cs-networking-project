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
// U sustini wrappuje funkcije iz ovih biblioteka
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
		return *this;
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
		if (conn < 0) {
			if (sock_id != -1) {
				shutdown(sock_id, 0);
				sock_id = -1;
			}
		}

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
// Konekcija je jednostavna, kada klijent posalje podatke
// zatvara svoj kraj konekcije, zatim ceka odgovor.
struct SimpleConnection : private Socket {

	SimpleConnection(const string& address, int port = 80) :
		Socket(Socket::get_client(address, port)) {}

	string query(const string& str) {
		send(str);
		finish_sending();
		return receive();
	}
};

// Slusa za zahteve na datom portu i odgovara na upite.
// Moguce su paralelne konekcije
struct SimpleParallelServer {

	// Prima podatke sa socketa, zove funkciju, salje odgovor
	// i zatvara socket. 
	template<class T>
	static void serve(int sock_id, T cb) {
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

struct StreamingSocket {

	static const char ESC = 0xab;
	static const char EOB = 0xcd;

	int sock_id;

	explicit StreamingSocket(int sock_id) : sock_id(sock_id) {}

	// Posalji jedan blok podataka
	void send(const string& data) {
		if (sock_id == -1) return;

		size_t size = 0;
		for (size_t i = 0; i < data.size(); i++) {
			size++;
			if (data[i] == ESC || data[i] == EOB) {
				size++;
			}
		}
		++size;
		char* p = new char[size]; // +1 je za poslednji EOB

		size_t j = 0;
		for (size_t i = 0; i < data.size(); i++) {
			if (data[i] == ESC || data[i] == EOB) {
				p[j++] = ESC;
			}
			p[j++] = data[i];
		}
		p[j] = EOB;
		
		size_t sent = 0;
		while (sent < size) {
			ssize_t status = ::send(sock_id, p+sent, size-sent, 0);
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

	StreamingSocket& operator= (StreamingSocket&& other) {
		tidy();
		sock_id = other.sock_id;
		other.sock_id = -1;
		return *this;
	}

	// Buffer sadrzi parsovane charove
	string recv_buffer;

	// Granicni slucaj: da li imamo neobradjen escape karakter na kraju bafera
	bool recv_is_escaped;

	template<class T>
	void push_to_buffer(const char* begin, const char* end, T cb) {
		while (begin != end) {
			if (recv_is_escaped) {
				recv_buffer += *begin++;
				recv_is_escaped = false;
			} else {
				if (*begin == ESC) {
					recv_is_escaped = true;
				} else if (*begin == EOB) {
					cb(recv_buffer);
					recv_buffer.clear();
				} else {
					recv_buffer += *begin;
				}
				++begin;
			}
		}
	}	

	// Pocni da primas podatke. Kada druga strana zatvori konekciju ili
	// se konekcija izgubi, funkcija ce se zavrsiti. U suprotnom, funkcija
	// puni svoj bafer, i kada ocita kraj jednog bloka, zove callback
	// funkciju sa obradjenim podacima
	template<class T>
	void receive(T cb, int buf_size = 4096) {
		if (sock_id == -1) return;

		char* buffer = new char[buf_size];
		recv_buffer.clear();
		recv_is_escaped = false;
		
		while (1) {
			ssize_t status = recv(sock_id, buffer, buf_size, 0);
			if (status <= 0) {
				delete[] buffer;
				return;
			} else {
				push_to_buffer(buffer, buffer+status, cb);
			}
		}
	}

	void tidy() {
		if (sock_id != -1) {
			shutdown(sock_id, 0);
		}
	}

	~StreamingSocket() {
		tidy();
	}

	// Zvati kad hocemo da zatvorimo jednu stranu konekcije
	// odnosno kad necemo vise da saljemo blokove
	void finish_sending() {
		shutdown(sock_id, SHUT_WR);
	}

};

// Template parametri:
// T - objekat za sinhronizaciju. Za svaki socket se pravi tacno jedan.
//	treba da primi StreamingSocket& parametar
// U - funkcija koja se zove kada se primi blok.
//	primer: receive(T* obj, const string& str);
// V - funkcija koja se zove na pocetku, koja managuje sinhronizaciju
// i takodje koristi socket za slanje
//	primer: run(T* obj);

struct StreamingParallelServer {

	// Prima podatke sa socketa, zove funkciju, salje odgovor
	// i zatvara socket. 
	template<class T, class U, class V>
	static void serve(int sock_id, U receive_func, V run_func) {
		StreamingSocket sock(sock_id);
		T obj(sock);
		auto recv_lambda = [&](const string& str) {
			receive_func(&obj, str);
		};
		thread t1(run_func, &obj);
		// loop za primanje blokova
		sock.receive(recv_lambda);
		t1.join();
	}

	int port;

	StreamingParallelServer(int port) : port(port) {}

	template<class T, class U, class V>
	void run(U receive_func, V run_func) {
		int server_sock = Socket::get_server(port);
		while (1) {
			// sockai remote;
			// socklen_t len = sizeof(sockai);
			// int sock = accept(server_sock, (socka*)&remote, &len);
			int sock = accept(server_sock, nullptr, nullptr);
			thread t(serve<T, U, V>, sock, receive_func, run_func);
			t.detach();
		}
	}
};
