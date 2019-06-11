int EO();

int bar1();
int bar2();

int foo() {
	int err = bar1();
	if (err) {
		goto fail;
	}
	err = bar2();
	if (err) {
		goto fail;
	}

	return 0;

fail:
	EO();
	return err;
}
