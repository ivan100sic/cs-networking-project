#define DEBUG
#include "common.hpp"

#include <iostream>
#include <string>
#include <algorithm>
using namespace std;

string broji_palindrome(const string& str) {
	// uzasna slozenost ali to je tacno ono sto mi treba
	long long sol = 0;
	int n = str.size();
	for (int i=0; i<n; i++) {
		for (int j=i+1; j<n; j++) {
			string part = str.substr(i, j-i);
			string rev = part;
			reverse(rev.begin(), rev.end());
			if (part == rev) {
				sol++;
			}
		}
	}
	return to_string(sol);
}

string eho(const string& str) {
	return str;
}

int main() {
	SimpleParallelServer server(3333);
	server.run(broji_palindrome);
}