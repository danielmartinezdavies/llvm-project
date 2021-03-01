#include <vector>

int main() {

	std::vector<int> a(10);

	for(auto i = std::begin(a); i != std::end(a); i++){
				*i = 0;
	}
}