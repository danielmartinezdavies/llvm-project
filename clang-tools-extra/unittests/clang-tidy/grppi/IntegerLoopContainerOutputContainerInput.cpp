#include <vector>

int main() {
	std::vector<int> a(10);
	std::vector<int> b(10);
	for(int i = 0; i < 10; i++){
		a[i] = b[i];
	}
}