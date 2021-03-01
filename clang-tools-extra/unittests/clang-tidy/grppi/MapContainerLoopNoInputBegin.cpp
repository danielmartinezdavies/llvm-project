#include <vector>

int main() {

	std::vector<int> a(10);

	for(auto i = a.begin(); i != a.end(); i++){
		*i = 0;
	}
}