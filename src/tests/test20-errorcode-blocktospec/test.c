// Assumes -5 is error code

int foo();

int main() {
	int err = foo();
	if (foo > 0) {
		return -5;
	}	
	return 0;
}

// EXPECTED: foo > 0
