#ifdef CS333_P3
#include "user.h"

void
roundRobinTest(){

  int proc;
  char next;

  proc = fork();
  if (proc == 0){
    while((proc = fork()) == 0);
    if(proc < 0){
      printf("Maximum processes reached. Press key to continue.\n");
      scan("%c", next);
      wait();
      exit();
    }
  }
}

int
main(int argc, char *argv[]){
  roundRobinTest();
}
#endif
