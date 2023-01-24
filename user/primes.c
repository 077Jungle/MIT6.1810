#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void primes_check(int *p_left) {
  int prime;
  int num;
  int p_right[2];
  
  if (read(p_left[0], &num, sizeof(int)) > 0) {
    prime = num;
    printf("prime %d\n", num);
    pipe(p_right);
    num = prime;
    if (fork() == 0) {
      close(p_left[0]);
      close(p_right[1]);
      primes_check(p_right);
    } else {
      close(p_right[0]);
      while(read(p_left[0], &num, sizeof(int)) > 0) {
        if (num % prime != 0) {
          write(p_right[1], &num, sizeof(int));
        }
      }
      close(p_left[0]);
      close(p_right[1]);
    }
  }

  wait((int *) 0);
}

int
main(int argc, char *argv[])
{
  int p_right[2];
  
  pipe(p_right);

  for (int num=2; num<=35; num++) {
    write(p_right[1], &num, sizeof(int));
  }
  close(p_right[1]);
  primes_check(p_right);
  exit(0);
}