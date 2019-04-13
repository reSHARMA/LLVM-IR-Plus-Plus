volatile int i = 2;

struct A
{
	virtual void f() { i--; }
};

//__attribute__((noinline, noclone))
void foo(A *a)
{
	a->f();
	i--;
}

int main()
{
	A a;
	foo(&a);
	a.f();
	return i;
}
