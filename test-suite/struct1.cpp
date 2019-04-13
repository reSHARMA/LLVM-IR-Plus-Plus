struct A{
	int a;
};

int main(){
	A *aa;
	A b;
	aa = &b;
	aa -> a = 9;
	return 0;
}
