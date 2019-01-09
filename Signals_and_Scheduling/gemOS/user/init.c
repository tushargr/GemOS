#include<init.h>
#include<memory.h>
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

static void signal(u64 signum, void* addr)
{
  _syscall2(SYSCALL_SIGNAL, signum,(long)addr) ;
}

static void alarm(u32 num)
{
  _syscall1(SYSCALL_ALARM, num) ;
}

static void sleep(u32 ticks)
{
  _syscall1(SYSCALL_SLEEP, ticks) ;
}

static void clone(void* func_addr,void* st_addr)
{
  _syscall2(SYSCALL_CLONE, (long)func_addr,(long)st_addr) ;
}

static char* expand(u64 size,u64 flags)
{
  return (char*)_syscall2(SYSCALL_EXPAND,size,flags);
}

static void fun2()
{
    sleep(15);
    for(int i=0;i<1000;i++){
      int k=0;
    }
    exit(0);
}

static void fun1()
{
  int x=1;
  
  x++;
  for(int i=0;i<10000;i++){
      int k=0;
    }
  exit(x);
  
}

static void fun3()
{
  int x=1;
  for(int i=0;i<10000;i++){
      int k=0;
    }
  exit(x);
  write("wrong\n",6);
}

static void fun4()
{
  sleep(20);
  for(int i=0;i<1000;i++){
      int k=0;
    }
    int d = 2/0;

  exit(0);
  
}
static int main()
{
    int x=1;
    x++;
    char* p1 = (char*)expand(5,1);
    clone(fun1,p1+4088);
    char* p2 = expand(5,1);
    clone(fun2,p2+4088);
    char* p3 = expand(5,1);
    clone(fun3,p3+4088);
    char* p4 = expand(5,1);
    clone(fun4,p4+4088);
    sleep(20);
    for(int i=0;i<1000;i++){
      int k=0;
    }

    sleep(10);
    exit(x);
}
static int main()
{
  unsigned long i, j;
  unsigned long buff[4096];
  i = getpid();
 
  for(i=0; i<4096; ++i){
      j = buff[i];
  }
  i=0x100034;
  j = i / (i-0x100034);
  exit(-5);
  return 0;
}
