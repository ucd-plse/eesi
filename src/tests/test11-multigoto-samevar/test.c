int EO();

int bar();
int baz();

int foo() {
	int err = bar();
	if (err) {
		goto fail2;
	}
	err = baz();
	if (err) {
		goto fail;
	}

	return 0;

fail2:
	EO();
fail:
	EO();
	return err;
}
