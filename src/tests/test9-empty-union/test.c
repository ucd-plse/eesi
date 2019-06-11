// error-only function
void EO();
int bar();

int main() {
	int ret = bar();

	if (ret <= 0) {
	}
	EO();
}
