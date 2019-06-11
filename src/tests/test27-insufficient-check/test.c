int mustcheck();

int main() {
  int err = mustcheck();
  // mustcheck <=0
  if (err < 0) {
	return -1;
  }
  return 0;
}
