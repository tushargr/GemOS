#include<init.h>
static void exit(int);
static int main(void);


void init_start()
{
  int retval = main();
  exit(0);
}

/*Invoke system call with no additional arguments*/
static long _syscall0(int syscall_num)
{
  asm volatile ( "int $0x80;"
                 "leaveq;"
                 "retq;"
                :::"memory");
  return 0;   /*gcc shutup!*/
}

/*Invoke system call with one argument*/

static long _syscall1(int syscall_num, int exit_code)
{
  asm volatile ( "int $0x80;"
                 "leaveq;"
                 "retq;"
                :::"memory");
  return 0;   /*gcc shutup!*/
}
/*Invoke system call with two arguments*/

static long _syscall2(int syscall_num, u64 arg1, u64 arg2)
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

static long getpid()
{
  return(_syscall0(SYSCALL_GETPID));
}

static long write(char *ptr, int size)
{
   return(_syscall2(SYSCALL_WRITE, (u64)ptr, size));
}

static long signal(int signum, unsigned long handler)
{
   return(_syscall2(SYSCALL_SIGNAL, signum, (u64)handler));
}

static long alarm(int ticks)
{
   return(_syscall1(SYSCALL_ALARM, ticks));
}

static long sleep(int ticks)
{
   return(_syscall1(SYSCALL_SLEEP, ticks));
}

static void alarm_handler(){
  write("alarm done!!\n",13);
}

// static int var = 0;
static void segv_handler(){
  write("segv handled\n",13);
  // var++;
  // if(var==2)exit(1);
  // exit(1);
}

// int val = 0;
static void fpe_handler(){
  write("fpe handled\n",12);
  // val++;
  // if(val==2)exit(1);
  // exit(1);
}

static int main()
{
  unsigned long i, j;
  unsigned long buff[4096];
  i = getpid();
  // signal(SIGALRM,(unsigned long)alarm_handler);
  // signal(SIGSEGV,(unsigned long)segv_handler);
  signal(SIGFPE,(unsigned long)fpe_handler);
  // alarm(3);
  // while(1);
  // int var = 100;
  // while(var)var++;
  // exit(0);
  // j = buff[5000];
  sleep(5);
  // alarm(2);
  // int x = alarm(5);
  // if(x==1)write("yo\n",3);
  // while(1);
  for(i=0; i<4096; ++i){
      j = buff[i];
  }
  // i=0x100034;
  // j = i / (i-0x100034);
  write("kello\n",6);
  sleep(5);
  write("mello\n",6);
  exit(-5);
  return 0;
}

