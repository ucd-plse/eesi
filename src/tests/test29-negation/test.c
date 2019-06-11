int mustcheck();

int main() {
  int ret = -1;
  if (mustcheck() != 1) {
	goto err;
  } 
  ret = 0;
err:
  return ret;
}
