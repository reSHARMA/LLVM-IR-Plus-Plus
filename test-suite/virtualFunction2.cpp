#include <stdio.h>

class TestVirtual
{
public:
  virtual void A();
  virtual int B(int x, int y);

};

void TestVirtual::A()
{
  printf("A\n");
}

int TestVirtual::B(int x, int y) 
{
  return x * x + 2 * x * y + y * y;
}

//void CallTestVirtual(TestVirtual* x)
//{
//  x->A();
//  printf("x->B(3, 5) = %d\n", x->B(3, 5));
//}


int main()
{
  TestVirtual* x = new TestVirtual();
  printf("x->B(3, 5) = %d\n", x->B(3, 5));
  x->A();
}

