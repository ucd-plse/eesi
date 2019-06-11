int mustcheck();

int foo() {
	int err = mustcheck();
	if (err < 0) {
		return err;
	}
	return 0;
}
