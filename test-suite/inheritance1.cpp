class Parent 
{ 
    public: 
      int id_p; 
}; 
   
// Sub class inheriting from Base Class(Parent) 
class Child : public Parent 
{ 
    public: 
      int id_c; 
}; 
  
//main function 
int main()  
   { 
        Child obj1; 
        obj1.id_c = 7; 
        obj1.id_p = 91; 
        return 0; 
   }
