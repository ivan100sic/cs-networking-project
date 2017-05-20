#define DEBUG
#include "common.hpp"
#include <queue>
#include <thread>
#include <mutex>
#include <chrono>
using namespace std;
using namespace std::chrono;

namespace slow_echo_server {
	
	struct State {
		queue<string> q;
		mutex mtx;
		StreamingSocket& sock;

		State(StreamingSocket& sock) : sock(sock) {}
	};

	void receive(State* obj, const string& str) {
		unique_lock<mutex> lock(obj->mtx);
		obj->q.push(str);
	}

	void run(State* obj) {
		while (1) {
			unique_lock<mutex> lock(obj->mtx);
			if (obj->q.empty()) {
				lock.unlock();
				this_thread::sleep_for(milliseconds(250));
			} else {
				string str = obj->q.front(); obj->q.pop();
				for (char x : str) {
					this_thread::sleep_for(milliseconds(25));
					obj->sock.send(string(1, x));
				}
			}
		}
	}

}

int main() {
	using namespace slow_echo_server;

	StreamingParallelServer server(3333);
	server.run<State>(receive, run);
}
