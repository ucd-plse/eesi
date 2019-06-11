int mustcheck();

int main() {
  int err = mustcheck();
  // mustcheck <=0
  // not a bug!
  if (err < 0) {
	return -1;
  } else if (err == 0) {
	return -2;	
  }
  return 0;
}
