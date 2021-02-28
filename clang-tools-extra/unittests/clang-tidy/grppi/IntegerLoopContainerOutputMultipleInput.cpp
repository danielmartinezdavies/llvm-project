#include <vector>

int main() {
	int array[10];
	int *pointer = new int[10];
	std::vector<int> a(10);
	std::vector<int> b(10);

	for(int i = 0; i < 10; i++){
		a[i] = b[i] + pointer[i] + array[i];
	}
}