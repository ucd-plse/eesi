int EO();

int bar();
int baz();

int foo() {
	int err = bar();
	if (err) {
		EO();
	}
	err = baz();
	if (err) {
		EO();
	}

	return 0;
}
