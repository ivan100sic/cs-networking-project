#include "common.hpp"
using namespace std;

int main() {
	// ajde da napravimo nesto ad-hoc
	StreamingSocket ssock(Socket::get_client("10.1.1.213", 3333));

	if (ssock.sock_id == -1) {
		cerr << "Failed to connect!\n";
		return 0;
	}

	auto recv_callback = [&](const string& str) {
		cout << str;
		cout.flush();
	};

	thread t1([&]() {
		ssock.receive(recv_callback);
	});

	t1.detach();

	while (cin) {
		string str;
		cin >> str;
		ssock.send(str + "\n");
	}
}
