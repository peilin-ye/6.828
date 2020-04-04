#include "types.h"
#include "stat.h"
#include "user.h"

void periodic();
void attack();

int
main(int argc, char *argv[])
{
  // test();
  attack();
}

void
test()
{
  int i;
  printf(1, "alarmtest starting\n");
  alarm(10, periodic);
  for(i = 0; i < 25*5000000; i++){
    if((i % 250000) == 0)
      write(2, ".", 1);
  }
  printf(1, "\n");
  exit();
}

void
attack()
{
  int i;
  printf(1, "your computer is gonna crash, muahahah!\n");
  alarm(1, periodic);
  
  asm ("mov    0xf0100004,%esp");

  for(;;)
    i++;
}

void
periodic()
{
  printf(1, "alarm!\n");
}