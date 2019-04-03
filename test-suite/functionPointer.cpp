
int add(int a, float f) {
	a = a + 1;
	return a + 1;
}

int main(int argc, char **argv) {
  int (*KKKKK)(int, float);
  KKKKK = add;
  KKKKK(10, 20.5);
  return 0;
}
