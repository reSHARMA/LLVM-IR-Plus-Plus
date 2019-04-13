struct A{
	virtual int foo (void) {return 42;}
	virtual int foo1 (void) {return foo();}
	virtual int foo2 (void) {return foo1();}
	virtual int foo3 (void) {return foo2();}
	virtual int foo4 (void) {return foo3();}
	virtual int foo5 (void) {return foo4();}
	virtual int foo6 (void) {return foo5();}
	virtual int foo7 (void) {return foo6();}
	virtual int foo8 (void) {return foo7();}
	virtual int foo9 (void) {return foo8();}
	virtual int foo10 (void) {return foo9();}
	virtual int foo11 (void) {return foo10();}
	virtual int foo12 (void) {return foo11();}
	virtual int foo13 (void) {return foo12();}
	virtual int foo14 (void) {return foo13();}
	virtual int foo15 (void) {return foo14();}
	virtual int foo16 (void) {return foo15();}
	virtual int foo17 (void) {return foo16();}
	virtual int foo18 (void) {return foo17();}
	virtual int foo19 (void) {return foo18();}
	virtual int foo20 (void) {return foo19();}
	virtual int foo21 (void) {return foo20();}
	virtual int foo22 (void) {return foo21();}
	virtual int foo23 (void) {return foo22();}
	virtual int foo24 (void) {return foo23();}
	virtual int foo25 (void) {return foo24();}
	virtual int foo26 (void) {return foo25();}
	virtual int foo27 (void) {return foo26();}
	virtual int foo28 (void) {return foo27();}
	virtual int foo29 (void) {return foo28();}
	virtual int foo30 (void) {return foo29();}
};

int test() {
	struct A a, *b = &a;
	return b -> foo9();
}
