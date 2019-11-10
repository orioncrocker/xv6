/* writen by orion crocker
/  for CS333_P3
*/

#ifdef CS333_P3
#include "types.h"
#include "user.h"

void
zombieTest(int n){

  int i, ret;
  int extra = 0;
  char *next = 0;

  printf(1, "\nZOMBIE TEST\n");

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
         "Verify with ctl+z and enter any key to continue.\n", n);
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

  printf(1, "\nSLEEP TEST\n");

  int ret = fork();

  if(ret == 0){
    int count = 0;
    while((ret = fork()) == 0)
      count++;
    if(ret > 0){
      wait();
      exit();
    }
    printf(1, "Created %d sleeping procs. "
           "Verify with ctl+s and enter any key to continue.\n", count);
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

  printf(1, "\nRUN QUEUE TEST\n");

  printf(1, "Press any key to begin test. Use ctl+r over the next few seconds.\n");
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

  if(extra)
    n = n-extra;

  for(i = 0; i < n; i++)
    wait();

  printf(1,"\n");
}

void
killTest(void){

  int ret;
  char *next = 0;

  printf(1, "\nKILL TEST\n");

  ret = fork();

  // child process
  if(ret == 0){
    int pid = getpid();
    printf(1, "Proc %d has been created and put to sleep. "
           "Verify %d is sleeping with ctl+s, then enter any key to kill it.\n", pid, pid);

    sleep(1000000000);
    printf(1, "You waited too long! %d woke up and killed itself.\n", pid);

  // parent process
  } else {

    gets(next, 10);
    kill(ret);

    printf(1, "Proc %d should now be a zombie. "
          "Verify with ctl+z and any key to continue.\n", ret);
    gets(next, 10);
    wait();
  }
}

int
main(int argc, char *argv[]){

  // default number of procs to test
  int n = 10;

  if(argc <= 1){
    zombieTest(n);
    runTest(n);
    sleepTest();
    killTest();
  } else {

    for (uint i = 1; i < argc; i++) {
      if (strcmp(argv[i], "zombie") == 0 || strcmp(argv[i], "z") == 0)
        zombieTest(n);
      else if (strcmp(argv[i], "run") == 0 || strcmp(argv[i], "r") == 0)
        runTest(n);
      else if (strcmp(argv[i], "sleep") == 0 || strcmp(argv[i], "s") == 0)
        sleepTest();
      else if (strcmp(argv[i], "kill") == 0 || strcmp(argv[i], "k") == 0)
        killTest();
      else
        printf(1, "Command not recognized, check p3-test.c for examples\n");
    }
  }
  printf(1, "TESTS COMPLETE\n");
  exit();
}
#endif
