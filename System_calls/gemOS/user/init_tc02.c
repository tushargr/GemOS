#include<init.h>
static void exit(int);
static int main(void);


void init_start()
{
  int retval = main();
  exit(0);
}

/*Invoke system call with no additional arguments*/
static int _syscall0(int syscall_num)
{
  asm volatile ( "int $0x80;"
                 "leaveq;"
                 "retq;"
                :::"memory");
  return 0;   /*gcc shutup!*/
}

/*Invoke system call with one argument*/

static int _syscall1(int syscall_num, int exit_code)
{
  asm volatile ( "int $0x80;"
                 "leaveq;"
                 "retq;"
                :::"memory");
  return 0;   /*gcc shutup!*/
}

/*Invoke system call with two arguments*/

static long _syscall2(int syscall_num, unsigned long arg1, unsigned long arg2)
{
  asm volatile ( "int $0x80;"
                 "leaveq;"
                 "retq;"
                :::"memory");
  return 0;   /*gcc shutup!*/
}


static void exit(int code)
{
  _syscall1(SYSCALL_EXIT, code); 
}

static int getpid()
{
  return(_syscall0(SYSCALL_GETPID));
}

#define MAP_RD  0x0
#define MAP_WR  0x1

static int write(const char *str, unsigned long len) {
  return _syscall2(SYSCALL_WRITE, (unsigned long)str, len);
}

static void *expand(unsigned size, int flags) {
	return (void *)_syscall2(SYSCALL_EXPAND, size, flags);
}

static void *shrink(unsigned size, int flags) {
	return (void *)_syscall2(SYSCALL_SHRINK, size, flags);
}

static void print_hex(long num) {
	if (num == 0)
		write("0", 1);
	else while (num) {
		int x = num % 16;
		char c = x < 10 ? '0' + x : 'A' - 10 + x;
		write(&c, 1);
		num /= 16;
	}
	write("\n", 1);
}

float tc2() { // check RW segment
  float marks = 0;
  long ptr;
  ptr = (long) expand(512, MAP_WR);
  if (ptr == 0x180001000)
	  marks += 0.0;
  else {
	  write("tc2.1 failed\n", sizeof("tc2.1 failed\n"));
	  return marks;
  }
  ptr = (long) expand(512, MAP_WR);
  if (ptr == 0x180201000)
	  marks += 0.5;
  else {
	  write("tc2.2 failed\n", sizeof("tc2.2 failed\n"));
	  return marks;
  }
  ptr = (long) shrink(256, MAP_WR);
  if (ptr == 0x180301000)
	  marks += 0.0;
  else {
	  write("tc2.3 failed\n", sizeof("tc2.3 failed\n"));
	  return marks;
  }
  ptr = (long) shrink(256, MAP_WR);
  if (ptr == 0x180201000)
	  marks += 0.5;
  else {
	  write("tc2.4 failed\n", sizeof("tc2.4 failed\n"));
	  return marks;
  }
  return marks;
}

static int main()
{
  unsigned long i;
  float marks;
#if 0
  unsigned long arr[4096];
  arr[4095] = 45;
#endif
  char marks_str[][18] = {"tc02 marks = 0.0\n", "tc02 marks = 0.5\n", "tc02 marks = 1.0\n"};
#if 1
  i = getpid();
  marks = tc2();
  i = (int) 2 * marks;
  write(marks_str[i], 17);
#endif
  exit(-5);
  return 0;
}
