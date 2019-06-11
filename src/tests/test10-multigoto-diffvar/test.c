int EO();

int bar();
int baz();

int foo() {
	int err = bar();
	if (err) {
		goto fail2;
	}
	int ret = baz();
	if (ret) {
		goto fail;
	}

	return 0;

fail2:
	EO();
fail:
	EO();
	return err;
}
