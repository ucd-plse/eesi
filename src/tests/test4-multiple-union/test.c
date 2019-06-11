// error-only function
void EO();

void NEO();

int foo(int x) {
	if (x) {
		return 1;
	} else {
		return -1;
	}
}

void bar() {
	int err = foo(2);
	if (err > 0) {
		EO();
	}
	NEO();
}


int main() {
	int err = foo(4);
	if (err < 0) {
		EO();
	} else {
		NEO();
	}
	return 0;
}
