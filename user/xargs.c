#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXLEN 128

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(2, "Usage: xargs cmd...\n");
    exit(1);
  }

  char *args[MAXARG];

  for (int i=1; i<argc; i++) {
    args[i - 1] = malloc(strlen(argv[i]) + 1);
    strcpy(args[i - 1], argv[i]);
  }

  int count = argc - 1;
  char* buf = malloc(MAXLEN);
  int i = 0;

  while(read(0, buf + i, 1) > 0) {
    if (buf[i] == ' ') {
      buf[i] = 0;
      args[count++] = buf;
      buf = malloc(MAXLEN);
      i = 0;
    } else if (buf[i] == '\n') {
      buf[i] = 0;
      args[count] = buf;
      args[count + 1] = 0;
      if (fork() == 0) {
        exec(args[0], args);
        exit(1);
      } else {
        wait(0);
      }
      for (i = argc - 1; i <= count; i++) {
        free(args[i]);
      }
      count = argc - 1;
      buf = malloc(MAXLEN);
      i = 0;
    } else {
      i++;
    }
  }

  for (i = 0; i < argc - 1; i++) {
    free(args[i]);
  }
  
  exit(0);
}