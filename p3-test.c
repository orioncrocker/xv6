#ifdef CS333_P3
#include "types.h"
#include "user.h"

void
zombieTest(int n){

  int i;
  int extra = 0;
  char *next = 0;

  for(i = 0; i < n; i++){
    int ret = fork();
    if(ret == -1)
      extra++;
    if(ret == 0)
      exit();
  }

  if(extra){
    printf(1, "Created %d too many child procs, not enough space.\n", extra);
    n = n-extra;
  }

  printf(1, "%d zombies should exist. "
         "Check lists or press any key to reap child procs.\n", n);
  gets(next, 10);

  int reaped = 0;
  for(i = 0; i < n; i++){
    wait();
    reaped++;
  }
  printf(1, "%d procs reaped.\n", reaped);
}

void
sleepTest(void){

  int ret = fork();
  char *next = 0;

  if(ret == 0) {
    int count = 0;
    while((ret = fork()) == 0)
      count++;
    if(ret > 0){
      wait();
      exit();
    }
    printf(1, "%d sleeping procs should exist. "
           "Check lists or press any key to reap child procs.\n", count);
    gets(next, 10);
    exit();
  }
  wait();
}

int
main(int argc, char *argv[]){

  if(argc == 1){
    zombieTest(10);
    //runningTest(10);
  } else {
    int n = atoi(argv[1]);
    zombieTest(n);
  }
  sleepTest();
  exit();
}
#endif
