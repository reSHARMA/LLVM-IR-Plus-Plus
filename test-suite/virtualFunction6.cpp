int a;
struct X {
   X * foo;
   virtual void f() {
       a = 9;
   }
};

struct Y : X {
    X * foo;
    virtual void f() {
        a = 7;
    }
};

struct Z : Y {
    X * foo;
    virtual void f() {
        a = 4;
    }
};

int main(){
    X **p, **q;
    X *t, *x, *y, *z;
    q = &z;
    p = &z;
    x = new X;
    y = new X;
    *p = x;
    x->foo = new Y;
    y->foo = new Z;
    t = z->foo;
    t->f();
    return a;
}
