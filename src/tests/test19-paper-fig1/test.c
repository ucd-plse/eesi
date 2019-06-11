void EO();
int foo();
int bar();

void f() {
	int err = foo ();
	if (err == -1) {
		EO () ;
		return;
	} else if (err < 0) {
		return;
	}
}

int g() {
	int ret = foo();
	if (ret) {
		goto fail;
	}
	ret = bar() ;
	if (ret) {
		goto fail;
	}
	return 0;
fail:
	return 1;
}
