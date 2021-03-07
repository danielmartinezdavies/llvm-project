#include <vector>

int main() {
	int l1 = 0;
	int l2 = 5;
	std::vector<int> a(10);

	for(int i = 0; i < l2; i++){
		a[i] = 0;
	}

	for(int i = l1; i < 5; i++){
		a[i] = 0;
	}

	for(int i = l1; i < l2; i++){
		a[i] = 0;
	}


}