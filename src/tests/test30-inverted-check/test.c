// Returns 0 on error
int mustcheck();

void EO();

int foo() {
	if (mustcheck()) {
		EO();
		return -1;	
	}
	return 0;
}
