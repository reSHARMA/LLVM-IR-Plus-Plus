float printMe(const char* str) {
  return 2;
}
int main(int argc, char **argv) {
//int main() {
  int i = 10;
  float f;
  f = printMe("Hello\n");
  f = 2.3456;
  i = 22 + 33;
  switch(i) {
    case 10:
      i = 20;
      break;
    default:
      i = i + 21;
    case 20:
      i = i * 3;
    case 30:
      i = i;
      break;
  }
  switch(i) {
    default:
      i = i * 2;
  }
  return 0;
}
