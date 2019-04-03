int func(int arr[]) {
  return arr[0];
}
int main(int argc, char **argv) {
  int x = 20;
  int arr[x][10];
  ++x;
  int arr1[x+2+1];
  arr1[0] = 10;
  func(arr1);
  return 0;
}
