#include <vector>

int main() {
	float k = 0.f;
	std::vector<int> a(10);
	for(auto &elem : a){
		k += k;
	}
}