#include "common.hpp"

using namespace std;

int main() {
	Connection komp("127.0.0.1", 3333);
	int n;
	cin >> n;
	komp.send(string(n, 'a'));
	komp.finish_sending();
	cout << komp.receive();
}