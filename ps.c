#ifdef CS333_P2

#include "types.h"
#include "user.h"
#include "uproc.h"

// not permanent
#define MAX 16

int
main(void) {

  struct uproc *p = malloc(sizeof(struct uproc)*MAX);
  int procs = getprocs(MAX, p);

  for (int i = 0; i < procs; i++) {
    // calculate elapsed time
    uint elapsed = p[i].elapsed_ticks;
    uint elapsed_s = elapsed / 1000;
    uint elapsed_ms = elapsed % 1000;
    char* elapsed_zeros = "";
    if (elapsed_ms < 10)
      elapsed_zeros = "00";
    else if (elapsed_ms < 100)
      elapsed_zeros = "0";

    // calculate cpu elapsed time
    uint cpu_time = p[i].CPU_total_ticks;
    uint cpu_s = cpu_time / 1000;
    uint cpu_ms = cpu_time % 1000;
    char* cpu_zeros = "";
    if (cpu_ms < 10)
      cpu_zeros = "00";
    else if (cpu_ms < 100)
      cpu_zeros = "0";

    printf(1, "PID\tName\tUID\tGID\tPPID\tElapsed\tCPU\tState\tSize\n"
           "%d\t%s\t%d\t%d\t%d\t%d.%s%d\t%d.%s%d\t%s\t%d\n",
           p[i].pid, p[i].name, p[i].uid, p[i].gid, p[i].ppid,
           elapsed_s, elapsed_zeros, elapsed_ms, cpu_s, cpu_zeros, cpu_ms,
           p[i].state, p[i].size);
  }
  free(p);
  exit();
}
#endif
