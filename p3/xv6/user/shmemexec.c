#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int i;

  printf(1, "########## exec begins ##########\n");

  printf(1, "test access shared page 1 (read)\n");
  int* ptr1 = (int*)shmem_access(1);
  printf(1, "0x%x: %d [expected = 0x9F000: 111]\n", ptr1, *ptr1);

  printf(1, "test access shared page 0 (write and read)\n");
  int* ptr2 = (int*)shmem_access(0);
  *ptr2 = 888;
  printf(1, "0x%x: %d [expected = 0x9E000: 888]\n", ptr2, *ptr2);

  printf(1, "test count users of valid shared pages\n");
  for(i = 0; i < 4; i++)
    printf(1, "%d ", shmem_count(i));
  printf(1, "[expected = 1 2 0 1]\n");

  printf(1, "########## exec ends ##########\n");
  exit();
}
