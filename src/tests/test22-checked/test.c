int mustcheck();

int main() {
  int err = mustcheck();
  if (err < 0) {
    return -1; 
  }
  return 0;
}
