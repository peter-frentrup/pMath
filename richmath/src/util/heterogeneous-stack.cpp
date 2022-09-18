#include <util/heterogeneous-stack.h>


using namespace richmath;

#ifndef NDEBUG

static void debug_test_simple_stack() {
  HeterogeneousStack<void*> stack;
  
  {
    float &f = stack.push_new<float>();
    f = 3.14f;
  }
  
  {
    int &i = stack.push_new<int>();
    i = 42;
  }
  
  ARRAY_ASSERT(stack.contents.length() == 2);

  ARRAY_ASSERT(stack.get_top<int>() == 42);
  
  stack.pop<int>();
  
  ARRAY_ASSERT(stack.contents.length() == 1);
  ARRAY_ASSERT(stack.get_top<float>() == 3.14f);
  
  {
    float &f = stack.push_new<float>();
    f = 123.456f;
  }
  
  ARRAY_ASSERT(stack.contents.length() == 2);
  ARRAY_ASSERT(stack.get_top<float>() == 123.456f);
  
  stack.pop<float>();
  
  ARRAY_ASSERT(stack.contents.length() == 1);
  ARRAY_ASSERT(stack.get_top<float>() == 3.14f);
  
  stack.pop<float>();
  
  ARRAY_ASSERT(stack.contents.length() == 0);
}

static void debug_test_obj_stack() {
  static int obj_counter = 0;
  struct Obj {
    Obj() {
      ++obj_counter;
      data1 = 11;
      data2 = 22;
      data3 = 33.0;
    }
    ~Obj() {
      --obj_counter;
    }
    
    int data1;
    int data2;
    double data3;
  };
  
  obj_counter = 0;
  
  {
    HeterogeneousStack<uint64_t> stack;
    
    {
      Obj &obj = stack.push_new<Obj>();
      ARRAY_ASSERT(obj.data1 == 11);
      ARRAY_ASSERT(obj.data2 == 22);
      ARRAY_ASSERT(obj.data3 == 33.0);
      
      obj.data1 = 111;
      obj.data2 = 222;
      obj.data3 = 2.71828;
    }
    
    ARRAY_ASSERT(obj_counter == 1);
    ARRAY_ASSERT(stack.contents.length() == 2);
    
    ARRAY_ASSERT(stack.get_top<Obj>().data1 == 111);
    ARRAY_ASSERT(stack.get_top<Obj>().data2 == 222);
    ARRAY_ASSERT(stack.get_top<Obj>().data3 == 2.71828);
    
    {
      int &i = stack.push_new<int>();
      i = 4711;
    }
    
    ARRAY_ASSERT(obj_counter == 1);
    ARRAY_ASSERT(stack.contents.length() == 3);
    
    ARRAY_ASSERT(stack.get_top<int>() == 4711);
    
    {
      Obj &obj = stack.push_new<Obj>();
      ARRAY_ASSERT(obj.data1 == 11);
      ARRAY_ASSERT(obj.data2 == 22);
      ARRAY_ASSERT(obj.data3 == 33.0);
      
      obj.data1 = 666;
      obj.data2 = 543;
      obj.data3 = -654.321;
    }
    
    ARRAY_ASSERT(obj_counter == 2);
    ARRAY_ASSERT(stack.contents.length() == 5);
    
    ARRAY_ASSERT(stack.get_top<Obj>().data1 == 666);
    ARRAY_ASSERT(stack.get_top<Obj>().data2 == 543);
    ARRAY_ASSERT(stack.get_top<Obj>().data3 == -654.321);
    
    stack.pop<Obj>();
    
    ARRAY_ASSERT(obj_counter == 1);
    ARRAY_ASSERT(stack.contents.length() == 3);
    ARRAY_ASSERT(stack.get_top<int>() == 4711);
    
    stack.pop<int>();
    
    ARRAY_ASSERT(obj_counter == 1);
    ARRAY_ASSERT(stack.contents.length() == 2);
    ARRAY_ASSERT(stack.get_top<Obj>().data1 == 111);
    ARRAY_ASSERT(stack.get_top<Obj>().data2 == 222);
    ARRAY_ASSERT(stack.get_top<Obj>().data3 == 2.71828);
    
  }
  
  ARRAY_ASSERT(obj_counter == 1); // not popped => destructor not called
}

void richmath::debug_test_heterogeneous_stack() {
  debug_test_simple_stack();
  debug_test_obj_stack();
}

#endif
