// Tests that NULL (0) can be added to error spec

#define NULL ((void *)0)

void EO();

void *foo() {
	EO();
	return NULL;
}
