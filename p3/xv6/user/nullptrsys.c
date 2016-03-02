#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int status;
  int* int_ptr = 0;
  status = pipe(int_ptr);
  printf(1, "syscall with null int ptr: %d\n", status);
  char* char_ptr = 0;
  status = chdir(char_ptr);
  printf(1, "syscall with null char ptr: %d\n", status);
  exit(); 
} 
