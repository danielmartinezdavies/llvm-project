#include <vector>

int main() {
	float k = 0.f;
	std::vector<int> a(10);
	for(auto i = a.begin(); i != a.end(); i++){
		k += *i;
	}
}