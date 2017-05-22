#include "sound.hpp"
#include "common.hpp"
#include <vector>
#include <string>
#include <alsa/asoundlib.h>
using namespace std;
using namespace std::chrono;

int main(int argc, char** argv) {

	if (argc <= 1 || argc > 3) {
		cout << "soundclient <host ip> [<port>=3333]\n";
	}

	const char* host_ip = argv[1];
	int port = argc == 3 ? stoi(argv[2]) : 3333;

	StreamingSocket ssock(Socket::get_client(host_ip, port));
	if (ssock.sock_id == -1) {
		cerr << "Failed to connect!\n";
		return 0;
	}

	SoundPlayer player;

	auto recv_callback = [&](const string& str) {
		// cerr << "Audio block, bytes: " << str.size() << '\n';
		vector<int16_t> a;
		for (size_t i=0; i<str.size(); i+=2) {
			uint8_t b0 = str[i];
			uint8_t b1 = str[i+1];

			int val = (int)b1 * 256 + b0;
			int16_t x = val - 32768;
			a.push_back(10*x);
		}

		/*
		cerr << "Preview:";
		for (size_t i=0; i<8; i++) cerr << ' ' << a[i];
		cerr << '\n';
		*/

		player.add(a);
	};

	thread t1([&]() {
		ssock.receive(recv_callback);
	});
	t1.detach();

	player.run();

	cout << "Enjoy! Press enter to quit\n";
	string w;
	getline(cin, w);
}
