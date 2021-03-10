#include <vector>

int main() {
	int k = 0;
	std::vector<int> a(10);
	for(auto i = a.begin(); i != a.end(); i++){
		k *= *i;
	}
}