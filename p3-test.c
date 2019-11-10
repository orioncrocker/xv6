#ifdef CS333_P3
#include "types.h"
#include "user.h"

void
zombieTest(int n){

  int i, ret;
  int extra = 0;
  char *next = 0;

  for(i = 0; i < n; i++){
    ret = fork();
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

  char *next = 0;
  int ret = fork();

  if(ret == 0){
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

void
runTest(int n){

  int i, ret;
  int extra = 0;
  char *next = 0;

  printf(1, "Press any key to begin run test. Use ctl-r over the next few seconds.\n");
  gets(next, 10);

  for(i = 0; i < n; i++){
    ret = fork();
    if(ret == -1)
      extra++;
    if(ret == 0) {
      for(int i = 0; i < 100000000; i++);
      exit();
    }
  }


/*
  int ret = fork();
  if (ret == 0){
    while((ret = fork()) == 0);
    for(int i = 0; i < 10000000; i++);

    if(ret > 0){
      wait();
      exit();
    }
*/
  if(extra)
    n = n-extra;

  for(i = 0; i < n; i++)
    wait();

  printf(1, "Test complete. Press any key to continue.\n");
  gets(next, 10);
}

int
main(int argc, char *argv[]){

  // default number of procs to test
  int n = 10;

  if(argc <= 1){
    zombieTest(n);
    runTest(n);
    sleepTest();

  } else {

    for (uint i = 1; i < argc; i++) {
      if (strcmp(argv[i], "zombie") == 0 || strcmp(argv[i], "z") == 0)
        zombieTest(n);
      else if (strcmp(argv[i], "run") == 0 || strcmp(argv[i], "r") == 0)
        runTest(n);
      else if (strcmp(argv[i], "sleep") == 0 || strcmp(argv[i], "s") == 0)
        sleepTest();
      else
        printf(1, "Command not recognized, check p3-test.c for examples\n");
    }
  }
  exit();
}
#endif
