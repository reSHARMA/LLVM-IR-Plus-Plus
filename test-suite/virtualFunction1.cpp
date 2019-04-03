class Shape {
   protected:
      int width, height;

   public:
      Shape(int a = 0, int b = 0) {
         width = a;
         height = b;
      }
      
      // pure virtual function
      virtual int area() = 0;
};
class rec: public Shape {
	public:
		int area(){
			return 0;
		}
};
int main(){
	Shape *s;
	rec r;
	s =&r;
	return s -> area();
}
