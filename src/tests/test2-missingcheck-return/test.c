#include <stdlib.h>

int *mustcheck();

int *bar() {
	// Propagated, OK
	int *p = mustcheck(); 
	return p;
}

int main() {
	// Missing check
	bar();
	return 0;
}
