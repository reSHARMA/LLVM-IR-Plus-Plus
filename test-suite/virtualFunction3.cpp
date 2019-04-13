class A{
	public:
	int a;
	A(int r){
		a = r;
	}
	A(){
	
	}
	virtual int foo(){
		return a;
	}
};

class B : public A{
	public:
	int foo(){
		a = a * a * a * 2;
		return a;
	}
};

class C : public A{
	public:
	int foo(){
		a =  a * a * 2;
		return a;
	}
};

C aa;

void foo(A *q){
	q = &aa;
}

int main(){
	A *q; 
	B  p;
	q = &p;
	foo(q);
	return q -> foo();
}
