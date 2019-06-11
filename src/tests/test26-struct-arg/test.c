int* mustcheck();

struct r {
  int* x;
};


int foo(struct r* ss) {
  ss->x = mustcheck();
  return 0;
}

int bar(struct r* ss) {
  ss->x = mustcheck();
  if (!ss->x) {
    return -1;
  }
  return 0;
}

int main() {
  struct r *tmp;
  if (!foo(tmp)) { 
    return -1;
  }
  if (!bar(tmp)) { 
    return -1;
  }
  return 0;
}
