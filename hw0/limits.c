#include <stdio.h>
#include <sys/resource.h>

int main() {
  struct rlimit lim;
  printf("stack size: %ld\n", (long) RLIMIT_STACK);
  printf("process limit: %ld\n", (long) RLIMIT_NPROC);
  printf("max file descriptors: %ld\n", (long) RLIMIT_NOFILE - 1);
  return 0;
}

