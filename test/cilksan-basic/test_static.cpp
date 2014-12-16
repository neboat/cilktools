#include <stdio.h>

class Foo {
public:
  Foo() {
    fprintf(stderr, "call constructor.\n");
  }
  ~Foo() {
    fprintf(stderr, "call destructor.\n");
  }
};

Foo foo;
static Foo foo2;

int main() {
    fprintf(stderr, "Main.\n"); 
    return 0;
}

