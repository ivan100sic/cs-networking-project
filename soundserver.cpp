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
	srec.run([](vector<int16_t>&& a) {
		double tot = 0;
		for (int16_t x : a) {
			tot += (double)x*x;
		}
		tot = sqrt(tot / a.size());
		cout << "Pwr: " << tot << '\n';
	});
}

namespace ss_instance {
	SoundRecorder* ptr;

	size_t block_id = 0;
	map<size_t, vector<int16_t>> backlog;
	mutex mtx;

	void update(vector<int16_t>&& a) {
		unique_lock<mutex> lock(mtx);
		block_id++;
		backlog[block_id] = a;
		if (block_id >= 16) {
			backlog.erase(block_id - 16);
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
		} else if (next_block_id + 16 > ss_instance::block_id) {
			// ocitaj ovaj blok, posalji ga na socket
			vector<int16_t> data = ss_instance::backlog[next_block_id++];
			lock.unlock();

			string str;
			for (int16_t x : data) {
				// ocitaj kao two's complement i stavi (little endian)
				size_t y = (int)x + 32768;
				uint8_t b0 = y & 0xff;
				uint8_t b1 = (y >> 8) & 0xff;
				str.push_back(b0);
				str.push_back(b1);
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