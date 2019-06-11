// Returns 0 on error
int mustcheck();
int mustcheck2();

void EO();

int foo() {
	if (!mustcheck2()) {
		goto err;
	}

	if (mustcheck() == 0) {
		EO();
		return -1;	
	}
	return 0;

err:
	return -1;
}
