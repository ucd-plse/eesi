int mustcheck();

int main() {
  int err = mustcheck();
  if (err) {
    return -1;
  }
  return 0;
}
