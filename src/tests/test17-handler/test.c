void EO();
void NEO();
int foo();

int main() {
	int err = foo();
	if (err < 0) {
		EO();
	}
}

void bar() {
	int ret = foo();
	if (ret < 0) {
		return;
	}
	NEO();
}
