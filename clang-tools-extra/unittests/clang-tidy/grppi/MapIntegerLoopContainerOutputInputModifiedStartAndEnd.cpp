#include <vector>

int main() {
	int array[10];
	int *pointer = new int[10];
	std::vector<int> a(10);
	std::vector<int> b(10);

		for(int i = 5; i < 8; i++){
			a[i] = b[i] + pointer[i] + array[i];
		}
}