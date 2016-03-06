#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int i;

  printf(1, "********** parent begins **********\n");

  printf(1, "test access shared page 3 (write and read)\n");
  int* ptr1 = (int*)shmem_access(3);
  *ptr1 = 333;
  printf(1, "0x%x: %d [expected = 0x9F000: 333]\n", ptr1, *ptr1);
  printf(1, "test access shared page 1 (write and read)\n");
  int* ptr2 = (int*)shmem_access(1);
  *ptr2 = 111;
  printf(1, "0x%x: %d [expected = 0x9E000: 111]\n", ptr2, *ptr2);
  printf(1, "test access shared page 3 (read)\n");
  int* ptr3 = (int*)shmem_access(3);
  printf(1, "0x%x: %d [expected = 0x9F000: 333]\n", ptr3, *ptr3);
  printf(1, "test access invalid shared page 4\n");
  int* ptr4 = (int*)shmem_access(4);
  printf(1, "0x%x [expected = 0x0]\n", ptr4);

  printf(1, "test count users of valid shared pages\n");
  for(i = 0; i < 4; i++)
    printf(1, "%d ", shmem_count(i));
  printf(1, "[expected = 0 1 0 1]\n");
  printf(1, "test count users of invalid shared page 4\n");
  printf(1, "%d [expected = -1]\n", shmem_count(4));

  int pid = fork();
  if(pid > 0)
  {
    wait();
    printf(1, "********** child ends **********\n");

    printf(1, "test access shared page 2 (write and read)\n");
    int* ptr6 = (int*)shmem_access(2);
    *ptr6 = 222;
    printf(1, "0x%x: %d [expected = 0x9D000: 222]\n", ptr6, *ptr6);
    printf(1, "test access shared page 0 (read)\n");
    int* ptr7 = (int*)shmem_access(0);
    printf(1, "0x%x: %d [expected = 0x9C000: 888]\n", ptr7, *ptr7);

    printf(1, "test count users of valid shared pages\n");
    for(i = 0; i < 4; i++)
      printf(1, "%d ", shmem_count(i));
    printf(1, "[expected = 1 1 1 1]\n");

    printf(1, "********** parent ends **********\n");
  }
  else if(pid == 0)
  {
    printf(1, "********** child begins **********\n");

    printf(1, "test access shared pages with parent pointers (read)\n");
    printf(1, "0x%x: %d [expected = 0x9F000: 333]\n", ptr1, *ptr1);
    printf(1, "0x%x: %d [expected = 0x9E000: 111]\n", ptr2, *ptr2);
    printf(1, "0x%x: %d [expected = 0x9F000: 333]\n", ptr3, *ptr3);

    printf(1, "test count users of valid shared pages\n");
    for(i = 0; i < 4; i++)
      printf(1, "%d ", shmem_count(i));
    printf(1, "[expected = 0 2 0 2]\n");

    printf(1, "test pass pointer to shared page to syscall\n");
    char* myargv[2];
    myargv[0] = (char*)ptr1;
    strcpy(myargv[0], "shmemexec");
    myargv[1] = 0;
    exec(myargv[0], myargv);
  }
  else
  {
    printf(1, "fork failed\n");
  }
  exit();
}
