#ifdef CS333_P3
#include "types.h"
#include "user.h"

void
zombieTest(){

  int n;
  char *next = 0;

  for(n = 0; n < 10; n++){
    if(fork() == 0)
      exit();
  }
  printf(1, "%d zombies should exist. Check ctl-Z\n", n);
  gets(next, 10);
  for(n = 0; n < 10; n++)
    wait();
}

int
main(int argc, char *argv[]){
  zombieTest();
  exit();
}
#endif
