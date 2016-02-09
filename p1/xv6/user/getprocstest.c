#include "types.h"
#include "user.h"

int
main(int argc, char* argv[])
{
  int procs = getprocs();
  printf(1, "Current Processes: %d\n", procs);
  exit();
}
