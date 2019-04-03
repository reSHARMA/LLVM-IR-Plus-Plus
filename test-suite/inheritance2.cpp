class Vehicle { 
  public: 
    Vehicle() 
    { 
    } 
}; 
  
// sub class derived from two base classes 
class Car: public Vehicle{ 
  
}; 
  
// main function 
int main() 
{    
    // creating object of sub class will 
    // invoke the constructor of base classes 
    Car obj; 
    return 0; 
}
