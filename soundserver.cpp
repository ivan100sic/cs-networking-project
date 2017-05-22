#include "sound.hpp"
#include "common.hpp"
#include <vector>
#include <cmath>
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <map>
#include <alsa/asoundlib.h>
using namespace std;
using namespace std::chrono;

void demo() {
	SoundRecorder srec;
	srec.run([](vector<uint32_t>&& a) {
		double tot = 0;
		for (uint32_t x : a) {
			tot += (double)x*x;
		}
		tot = sqrt(tot / a.size());
		cout << "Pwr: " << tot << '\n';
	});
}

namespace ss_instance {
	SoundRecorder* ptr;

	const int BACKLOG_SIZE = 32;

	size_t block_id = 0;
	map<size_t, vector<uint32_t>> backlog;
	mutex mtx;

	void update(vector<uint32_t>&& a) {
		unique_lock<mutex> lock(mtx);
		block_id++;
		backlog[block_id] = a;
		if (block_id >= BACKLOG_SIZE) {
			backlog.erase(block_id - BACKLOG_SIZE);
		}
	}
}

struct State {
	StreamingSocket& sock;
	State(StreamingSocket& sock) : sock(sock) {}
};

// Pri uspostavljanju konekcije ne radimo nista
void ss_recv(State* obj, const string& str) {}

// Prava magija se desava u ovoj funkciji
void ss_run(State* obj) {
	size_t next_block_id;
	{
		unique_lock<mutex> lock(ss_instance::mtx);
		next_block_id = ss_instance::block_id;
	}

	while (1) {
		unique_lock<mutex> lock(ss_instance::mtx);
		if (next_block_id > ss_instance::block_id) {
			// poranili smo, cekamo da se pojavi ovaj blok
			lock.unlock();
			this_thread::sleep_for(milliseconds(5));
		} else if (next_block_id + ss_instance::BACKLOG_SIZE
				> ss_instance::block_id)
		{
			// ocitaj ovaj blok, posalji ga na socket
			vector<uint32_t> data = ss_instance::backlog[next_block_id++];
			lock.unlock();

			string str;
			for (uint32_t x : data) {
				uint8_t b0 = x & 0xff;
				uint8_t b1 = (x >> 8) & 0xff;
				uint8_t b2 = (x >> 16) & 0xff;
				uint8_t b3 = (x >> 24) & 0xff;
				str.push_back(b0);
				str.push_back(b1);
				str.push_back(b2);
				str.push_back(b3);
			}
			obj->sock.send(str);
		} else {
			// zavrsi, previse kasnis!
			break;
		}
	}
}

int main(int argc, char** argv) {
	int port = 3333;
	if (argc == 2) {
		port = stoi(argv[1]);
	}

	SoundRecorder instance;
	ss_instance::ptr = &instance;

	thread t([] {
		ss_instance::ptr->run(ss_instance::update);
	});
	t.detach();

	StreamingParallelServer sserver(port);
	sserver.run<State>(ss_recv, ss_run);

	return 0;	
}