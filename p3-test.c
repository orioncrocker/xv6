/********************************************************************************
* Author: Orion Crocker
* Filename: p3-test.c
* Date: 11/09/19
*
* Class: Test Suite for CS333_P3
* Description: Tests implmentation of process lists for PSU CS333
* Project 3. Assumes that user has already implemented Ctrl+ shortcuts
* to view lists.
********************************************************************************/

#ifdef CS333_P3
#include "types.h"
#include "user.h"

static void
zombieTest(int n){

  int i, ret;
  int extra = 0;
  char *next = 0;

  printf(1, "\n----------\nZOMBIE TEST\n----------\n");

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

  printf(1, "Test created %d zombie processes. "
         "Verify with Ctrl+Z and press enter to continue.\n", n);
  gets(next, 10);

  int reaped = 0;
  for(i = 0; i < n; i++){
    wait();
    reaped++;
  }
  printf(1,"%d processes reaped.\n** Test complete! **\n");
}

static void
sleepTest(void){

  char *next = 0;

  printf(1, "\n----------\nSlEEP TEST\n----------\n");

  int ret = fork();

  if(ret == 0){
    int count = 0;
    while((ret = fork()) == 0)
      count++;
    if(ret > 0){
      wait();
      exit();
    }
    printf(1, "Test created %d sleeping procs. "
           "Verify with Ctrl+S and press enter to continue.\n", count);
    gets(next, 10);
    exit();
  }
  wait();
  printf(1,"\n** Test complete! **\n");
}

static void
runTest(int n){

  int i, ret;
  int extra = 0;
  char *next = 0;

  printf(1, "\n----------\nROUND ROBIN TEST\n----------\n");

  printf(1, "Press enter to begin test. Use Ctrl+R over the next few seconds.\n");
  gets(next, 10);

  for(i = 0; i < n; i++){
    ret = fork();
    if(ret == -1)
      extra++;
    if(ret == 0) {
      for(int i = 0; i < 200000000; i++);
      exit();
    }
  }

  if(extra)
    n = n-extra;

  for(i = 0; i < n; i++)
    wait();

  printf(1,"\n** Test complete! **\n");
}

static void
killTest(void){

  int ret;
  char *next = 0;

  printf(1, "\n----------\nKILL TEST\n----------\n");

  ret = fork();

  // child process
  if(ret == 0){
    int pid = getpid();
    printf(1, "Proc %d has been created and put to sleep. "
           "Verify %d is sleeping with Ctrl+S, then press enter to kill it.\n", pid, pid);

    sleep(1000000000);
    printf(1, "You waited too long! %d woke up and killed itself.\n", pid);
    exit();
  }
  // parent process
  else {

    gets(next, 10);
    kill(ret);

    printf(1, "Proc %d should now be a zombie. "
          "Verify with Ctrl+Z and press enter to continue.\n", ret);
    gets(next, 10);
    wait();
    printf(1,"\n** Test complete! **\n");
  }
}

int
main(int argc, char *argv[]){

  // default number of procs to create in tests
  int n = 25;

  if(argc <= 1){
    zombieTest(n);
    runTest(n);
    sleepTest();
    killTest();
    printf(1, "\n\n** ALL TESTS COMPLETE **\n\n");
  } else {

    for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "zombie") == 0 || strcmp(argv[i], "z") == 0)
        zombieTest(n);
      else if (strcmp(argv[i], "run") == 0 || strcmp(argv[i], "r") == 0)
        runTest(n);
      else if (strcmp(argv[i], "sleep") == 0 || strcmp(argv[i], "s") == 0)
        sleepTest();
      else if (strcmp(argv[i], "kill") == 0 || strcmp(argv[i], "k") == 0)
        killTest();
      else
        printf(1, "Command not recognized, check p3-test.c's main() for examples\n");
    }
  }
  exit();
}
#endif
