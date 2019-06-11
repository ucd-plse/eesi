int foo1();
int foo2();
void EO();

int main() {
	int err1 = foo1();
	int err2 = foo2();
		
	// Test if foo1 succeeded
	if (!err1) {
		// Test if foo2 failed
		if (err2) {
			EO();
			return 1;
		}
	}
	
	return 0;
}


