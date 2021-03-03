#include <vector>

int main() {

	std::vector<int> a(10);
	for (auto elem: a) {
		elem = 0;
	}
}