class Base { };

class Sub : public Base {
public: 
        virtual void bar();
};

Sub foo;
Sub * const pointer = &foo;
Sub* function() { return &foo; };

int main() {
        // Gcc 4.8.2 devirtualizes this:
        pointer->bar();
        // but not this:
        function()->bar();
}
