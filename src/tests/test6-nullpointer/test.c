#include <stdlib.h>

void EO();
void NEO();


int* foo(int x) {
	int *array = malloc(x * sizeof(int));
	return array;
}

int main() {
	int *arr = foo(42);
	if (!arr) {
		EO();
	}
}
