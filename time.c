#ifdef CS333_P2
#include "types.h"
#include "user.h"

int
main(int argc, char *argv[]) {

  uint start = uptime();
  int pid = fork();

  if (pid > 0)
    wait();
  else if (pid == 0) {
    exec(argv[1],argv+1);
    exit();
  }

  uint stop = uptime();
  uint total = stop - start;
  uint total_s = total / 1000;
  uint total_ms = total % 1000;
  char* total_zeros = "";
  if (total_ms < 10)
    total_zeros = "00";
  else if (total_ms < 100)
    total_zeros = "0";

  printf(1, "%s ran in %d.%s%d seconds.\n",
         argv[1], total_s, total_zeros, total_ms);

  exit();
}
#endif
