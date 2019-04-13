class X{
	public:
		virtual void set(){
			int a = 9;
		}
};

class Y : public X{
	public:
		void set(){
			int s = 8;
		}
};

int main(){
	Y y;
	X* x;
	x = &y;
	x -> set();
	return 0;
}
