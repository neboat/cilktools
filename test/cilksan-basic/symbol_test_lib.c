

int __attribute__((weak)) bar(int n);
int __attribute__((weak)) baz(int n);


int foo(int n) {
  return 1 * bar(n) * baz(n);
}

int bar(int n) {
  return 3;
}

int baz(int n) {
  return 5;
}
