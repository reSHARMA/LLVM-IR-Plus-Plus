class printData {
   public:
      void print(int i) {
	      i = i + 1;
      }
      void print(double  f) {
	      f = f * 2;
      }
      void print(char* c) {
	      c[0] = '1';
      }
};

int main(void) {
   printData pd;
 
   // Call print to print integer
   pd.print(5);
   
   // Call print to print float
   pd.print(500.263);
   
   // Call print to print character
   pd.print("Hello C++");
 
   return 0;
}
