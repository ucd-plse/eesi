// error-only function
void EO();
int bar();

int main() {
	int ret;

	if ((ret = bar()) < 0) {
		EO();
		return -1;
	}
	return 0;
}
