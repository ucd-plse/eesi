int mustcheck();

struct s {
  int err;
};

int main() {
  struct s *tmp;
  tmp->err = mustcheck();
  if (tmp->err < 0) {
    return -1;
  }
  return 0;
}
