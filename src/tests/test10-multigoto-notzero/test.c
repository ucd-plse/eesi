int EO();

// <0 on error
int mustcheck();

// >0 on error
int invert() {
	if (mustcheck() < 0) {
		return 1;
	}
	return 0;
}

// !=0 on error
int foo() {
	int err = mustcheck();
	if (err) {
		goto fail2;
	}
	int ret = invert();
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
