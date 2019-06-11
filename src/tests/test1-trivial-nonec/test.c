// error-only function
void EO();

int foo(int x) {
	if (x) {
		EO();
		return -1;
	} else {
		return 1;
	}
}
