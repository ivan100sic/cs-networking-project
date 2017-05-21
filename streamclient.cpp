#include "common.hpp"
using namespace std;

int main() {
	// ajde da napravimo nesto ad-hoc
	StreamingSocket ssock(Socket::get_client("127.0.0.1", 3333));

	if (ssock.sock_id == -1) {
		cerr << "Failed to connect!\n";
		return 0;
	}

	auto recv_callback = [&](const string& str) {
		cout << str[0];
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
