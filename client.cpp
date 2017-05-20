#include "common.hpp"

using namespace std;

int main() {
	int n;
	cin >> n;
	cout << SimpleConnection(
		"127.0.0.1", 3333).query(string(n, 'a')) << '\n';
}
