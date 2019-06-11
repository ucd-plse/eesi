int foo1();
int foo2();
int foo3();
void EO();

int main() {
	int err = foo1();
	if (err) {
		goto err1;
	}
	err = foo2();
	if (err) {
		goto err2;
	}
	err = foo3();
	if (err) {
		goto err3;
	}

	return 0;
err1:
	EO();
err2:
	EO();
err3:
	EO();

	return 1;
}
