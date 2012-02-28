/* Tests the halt system call. */

#include "tests/lib.h"
#include "tests/main.h"
#include "stdio.h"

void
test_main (void) 
{
  write (1, &("boom"),4);
  printf ("BOOM");
  halt ();
  fail ("should have halted");
}
