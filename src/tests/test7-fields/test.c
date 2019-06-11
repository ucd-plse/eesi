struct point {
	int x;
	int y;
};

int mustcheck();

int foo() {
	struct point p;

	// Should not be reported
	p.x = mustcheck();
	if (p.x == 0) {
		return -1;
	} 
	
	// Should be reported
	p.y = mustcheck();

	return 0;	
}
