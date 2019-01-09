#include<context.h>
#include<memory.h>
#include<schedule.h>
#include<lib.h>
static u64 numticks;
static u64 to_free_pfn = 0;

void handle_timer_tick()
{
  u64 pop_top;
  asm volatile("push %rax;"
               "push %rbx;"
               "push %rcx;"
               "push %rdx;"
               "push %rsi;"
               "push %rdi;"
               "push %r8;""push %r9;""push %r10;""push %r11;"
               "push %r12;""push %r13;""push %r14;""push %r15;");
  asm volatile("mov %%rsp, %0;"
                :"=r" (pop_top));

  u64* bp;
  asm volatile("mov %%rbp, %0;"
               :"=r" (bp)
               ::"memory");

  printf("Got a tick. #ticks = %u\n", ++numticks);   /*XXX Do not modify this line*/
  
  u64 *urip = (u64 *)(bp+1);
  u64 *ustackp = (u64 *)(bp+4);
  struct exec_context *current = get_current_ctx();
  struct exec_context *list = get_ctx_list();

  for (int i = 0; i < MAX_PROCESSES; ++i){
    if(is_set(to_free_pfn,i)){
      os_pfn_free(OS_PT_REG,(list+i)->os_stack_pfn);
      clear_bit(to_free_pfn,i);
    }
  }

  for(int pid=0;pid<MAX_PROCESSES;pid++){
    if((list+pid)->state==WAITING){
      if((list+pid)->ticks_to_sleep!=0){
        (list+pid)->ticks_to_sleep--;
      }
      if((list+pid)->ticks_to_sleep==0){
        (list+pid)->state = READY;
      }
    }
  }

  int flag = 0;
  for(int pid=1;pid<MAX_PROCESSES;pid++){
    if((list+pid)->state==READY){
      flag = 1;
      break;
    }
  }
  if(flag==1){
    struct exec_context *next;
    int old_pid = current->pid;  

    for(int pid=1;pid<MAX_PROCESSES;pid++){
      if((list+((pid+old_pid)%MAX_PROCESSES))->state == READY){
        next = (list+((pid+old_pid)%MAX_PROCESSES));
        if(((pid+old_pid)%MAX_PROCESSES)==0)continue;
        break;
      }
    }
    
    printf("scheduling: old pid = %d  new pid  = %d\n", current->pid, next->pid); /*XXX: Don't remove*/
    current->state = READY;
    current->regs.entry_rip = *(bp+1);
    current->regs.entry_cs = *(bp+2);
    current->regs.entry_rflags = *(bp+3);
    current->regs.entry_rsp = *(bp+4);
    current->regs.entry_ss = *(bp+5);
    current->regs.rbp = *(bp);
    current->regs.r15 = *(u64*)(pop_top);
    current->regs.r14 = *((u64*)(pop_top)+1);
    current->regs.r13 = *((u64*)(pop_top)+2);
    current->regs.r12 = *((u64*)(pop_top)+3);
    current->regs.r11 = *((u64*)(pop_top)+4);
    current->regs.r10 = *((u64*)(pop_top)+5);
    current->regs.r9 = *((u64*)(pop_top)+6);
    current->regs.r8 = *((u64*)(pop_top)+7);
    current->regs.rdi = *((u64*)(pop_top)+8);
    current->regs.rsi = *((u64*)(pop_top)+9);
    current->regs.rdx = *((u64*)(pop_top)+10);
    current->regs.rcx = *((u64*)(pop_top)+11);
    current->regs.rbx = *((u64*)(pop_top)+12);
    current->regs.rax = *((u64*)(pop_top)+13);
    
    *(bp) = next->regs.rbp;
    *(bp+1) = next->regs.entry_rip;
    *(bp+2) = next->regs.entry_cs;
    *(bp+3) = next->regs.entry_rflags;
    *(bp+4) = next->regs.entry_rsp;
    *(bp+5) = next->regs.entry_ss;
    next->state = RUNNING;
    ack_irq();
    set_tss_stack_ptr(next);
    set_current_ctx(next);
    asm volatile("mov %0, %%r15;"
                ::"r"(next->regs.r15));
    asm volatile("mov %0, %%r14;"
                ::"r"(next->regs.r14));
    asm volatile("mov %0, %%r13;"
                ::"r"(next->regs.r13));
    asm volatile("mov %0, %%r12;"
                ::"r"(next->regs.r12));
    asm volatile("mov %0, %%r11;"
                ::"r"(next->regs.r11));
    asm volatile("mov %0, %%r10;"
                ::"r"(next->regs.r10));
    asm volatile("mov %0, %%r9;"
                ::"r"(next->regs.r9));
    asm volatile("mov %0, %%r8;"
                ::"r"(next->regs.r8));
    asm volatile("mov %0, %%rbx;"
                ::"r"(next->regs.rbx));
    asm volatile("mov %0, %%rcx;"
                ::"r"(next->regs.rcx));
    asm volatile("mov %0, %%rdx;"
                ::"r"(next->regs.rdx));
    asm volatile("mov %0, %%rsi;"
                ::"r"(next->regs.rsi));
    asm volatile("mov %0, %%rdi;"
                ::"r"(next->regs.rdi));
    asm volatile("mov %0, %%rax;"
                ::"r"(next->regs.rax));

    asm volatile("mov %%rbp, %%rsp;"
                 "pop %%rbp;"
                 "iretq;"
                 :::"memory");
  }

  if(current->alarm_config_time!=0){
    if(current->ticks_to_alarm!=0)current->ticks_to_alarm--;
    if(current->ticks_to_alarm==0){
      if(is_set(current->pending_signal_bitmap,SIGALRM)){
        clear_bit(current->pending_signal_bitmap,SIGALRM);
        current->alarm_config_time = 0;
        invoke_sync_signal(SIGALRM,ustackp,urip);
      }
    }
  }

  ack_irq();  /*acknowledge the interrupt, next interrupt */

  asm volatile("mov %0, %%rsp;"
               "pop %%r15;""pop %%r14;""pop %%r13;""pop %%r12;"
               "pop %%r11;""pop %%r10;""pop %%r9;""pop %%r8;"
               "pop %%rdi;""pop %%rsi;""pop %%rdx;""pop %%rcx;"
               "pop %%rbx;""pop %%rax;"
               :
               : "r" (pop_top)
               : "memory");

static u64 numticks;

static void save_current_context()
{
  /*Your code goes in here*/ 
} 

static void schedule_context(struct exec_context *next)
{
  /*Your code goes in here. get_current_ctx() still returns the old context*/
 struct exec_context *current = get_current_ctx();
 printf("schedluing: old pid = %d  new pid  = %d\n", current->pid, next->pid); /*XXX: Don't remove*/
/*These two lines must be executed*/
 set_tss_stack_ptr(next);
 set_current_ctx(next);
 return;
}

static struct exec_context *pick_next_context(struct exec_context *list)
{
  /*Your code goes in here*/
  
   return NULL;
}
static void schedule()
{
/* 
 struct exec_context *next;
 struct exec_context *current = get_current_ctx(); 
 struct exec_context *list = get_ctx_list();
 next = pick_next_context(list);
 schedule_context(next);
*/     
}

static void do_sleep_and_alarm_account()
{
 /*All processes in sleep() must decrement their sleep count*/ 
}

/*The five functions above are just a template. You may change the signatures as you wish*/
void handle_timer_tick()
{
 /*
   This is the timer interrupt handler. 
   You should account timer ticks for alarm and sleep
   and invoke schedule
 */
  printf("Got a tick. #ticks = %u\n", ++numticks);   /*XXX Do not modify this line*/ 
  ack_irq();  /*acknowledge the interrupt, next interrupt */
  asm volatile("mov %%rbp, %%rsp;"
               "pop %%rbp;"
               "iretq;"
               :::"memory");
}

void do_exit()
{
  u64* bp;
  asm volatile("mov %%rbp, %0;"
               :"=r" (bp)
               ::"memory");
  struct exec_context *current = get_current_ctx();
  struct exec_context *list = get_ctx_list();
  int flag = 0;
  for(int pid=1;pid<MAX_PROCESSES;pid++){
    if((list+pid)->state!=UNUSED && pid!=current->pid){
      flag = 1;
      break;
    }
  }
  if(flag==0){
    current->state = UNUSED;
    do_cleanup();
  }
  
  flag = 0;
  for(int pid=1;pid<MAX_PROCESSES;pid++){
    if((list+pid)->state==READY && pid!=current->pid){
      flag = 1;
      break;
    }
  }
  struct exec_context *next;
  int old_pid = current->pid;  

  for(int pid=1;pid<MAX_PROCESSES;pid++){
    if((list+((pid+old_pid)%MAX_PROCESSES))->state == READY){
      next = (list+((pid+old_pid)%MAX_PROCESSES));
      if(((pid+old_pid)%MAX_PROCESSES)==0)continue;
      break;
    }
  }

  if(flag == 0)next = list;
  printf("scheduling: old pid = %d  new pid  = %d\n", current->pid, next->pid);

  current->state = UNUSED;
  *(bp+1) = next->regs.entry_rip;
  *(bp+2) = next->regs.entry_cs;
  *(bp+3) = next->regs.entry_rflags;
  *(bp+4) = next->regs.entry_rsp;
  *(bp+5) = next->regs.entry_ss;
  *(bp) = next->regs.rbp;

  next->state = RUNNING;
  set_tss_stack_ptr(next);
  set_current_ctx(next);

  set_bit(to_free_pfn,current->pid);

  asm volatile("mov %0, %%r15;"
              ::"r"(next->regs.r15));
  asm volatile("mov %0, %%r14;"
              ::"r"(next->regs.r14));
  asm volatile("mov %0, %%r13;"
              ::"r"(next->regs.r13));
  asm volatile("mov %0, %%r12;"
              ::"r"(next->regs.r12));
  asm volatile("mov %0, %%r11;"
              ::"r"(next->regs.r11));
  asm volatile("mov %0, %%r10;"
              ::"r"(next->regs.r10));
  asm volatile("mov %0, %%r9;"
              ::"r"(next->regs.r9));
  asm volatile("mov %0, %%r8;"
              ::"r"(next->regs.r8));
  asm volatile("mov %0, %%rbx;"
              ::"r"(next->regs.rbx));
  asm volatile("mov %0, %%rcx;"
              ::"r"(next->regs.rcx));
  asm volatile("mov %0, %%rdx;"
              ::"r"(next->regs.rdx));
  asm volatile("mov %0, %%rsi;"
              ::"r"(next->regs.rsi));
  asm volatile("mov %0, %%rdi;"
              ::"r"(next->regs.rdi));
  asm volatile("mov %0, %%rax;"
              ::"r"(next->regs.rax));

  asm volatile("mov %%rbp, %%rsp;"
               "pop %%rbp;"
               "iretq;"
               :::"memory");
}

long do_sleep(u32 ticks)
{
  u64* bp;
  asm volatile("mov %%rbp, %0;"
               :"=r" (bp)
               ::"memory");
  
  struct exec_context *current = get_current_ctx();
  struct exec_context *list = get_ctx_list();
  current->ticks_to_sleep = ticks;
  current->state = WAITING;

  u64 ** old_bp = (u64 **)(bp);
  current->regs.r15 = *(*old_bp + 2);
  current->regs.r14 = *(*old_bp + 3);
  current->regs.r13 = *(*old_bp + 4);
  current->regs.r12 = *(*old_bp + 5);
  current->regs.r11 = *(*old_bp + 6);
  current->regs.r10 = *(*old_bp + 7);
  current->regs.r9 = *(*old_bp + 8);
  current->regs.r8 = *(*old_bp + 9);
  current->regs.rbp = *(*old_bp + 10);
  current->regs.rdi = *(*old_bp + 11);
  current->regs.rsi = *(*old_bp + 12);
  current->regs.rdx = *(*old_bp + 13);
  current->regs.rcx = *(*old_bp + 14);
  current->regs.rbx = *(*old_bp + 15);
  current->regs.entry_rip = *(*old_bp + 16);
  current->regs.entry_cs = *(*old_bp + 17);
  current->regs.entry_rflags = *(*old_bp + 18);
  current->regs.entry_rsp = *(*old_bp + 19);
  current->regs.entry_ss = *(*old_bp + 20);

  struct exec_context *next;
  int old_pid = current->pid;  
  int flag = 0;
  for(int pid=1;pid<MAX_PROCESSES;pid++){
    if((list+((pid+old_pid)%MAX_PROCESSES))->state == READY){
      next = (list+((pid+old_pid)%MAX_PROCESSES));
      if(((pid+old_pid)%MAX_PROCESSES)==0)continue;
      flag = 1;
      break;
    }
  }
  if(flag==0)next = list;
  printf("scheduling: old pid = %d new pid = %d\n",current->pid,next->pid);
  *(bp+1) = next->regs.entry_rip;
  *(bp+2) = next->regs.entry_cs;
  *(bp+3) = next->regs.entry_rflags;
  *(bp+4) = next->regs.entry_rsp;
  *(bp+5) = next->regs.entry_ss;
  *(bp) = next->regs.rbp;
  
  next->state = RUNNING;
  set_tss_stack_ptr(next);
  set_current_ctx(next);

  asm volatile("mov %0, %%r15;"
              ::"r"(next->regs.r15));
  asm volatile("mov %0, %%r14;"
              ::"r"(next->regs.r14));
  asm volatile("mov %0, %%r13;"
              ::"r"(next->regs.r13));
  asm volatile("mov %0, %%r12;"
              ::"r"(next->regs.r12));
  asm volatile("mov %0, %%r11;"
              ::"r"(next->regs.r11));
  asm volatile("mov %0, %%r10;"
              ::"r"(next->regs.r10));
  asm volatile("mov %0, %%r9;"
              ::"r"(next->regs.r9));
  asm volatile("mov %0, %%r8;"
              ::"r"(next->regs.r8));
  asm volatile("mov %0, %%rbx;"
              ::"r"(next->regs.rbx));
  asm volatile("mov %0, %%rcx;"
              ::"r"(next->regs.rcx));
  asm volatile("mov %0, %%rdx;"
              ::"r"(next->regs.rdx));
  asm volatile("mov %0, %%rsi;"
              ::"r"(next->regs.rsi));
  asm volatile("mov %0, %%rdi;"
              ::"r"(next->regs.rdi));
  asm volatile("mov %0, %%rax;"
              ::"r"(next->regs.rax));
  
  asm volatile("mov %%rbp, %%rsp;"
               "pop %%rbp;"
               "iretq;"
               :::"memory");
}

long do_clone(void *th_func, void *user_stack)
{
  u64* bp;
  asm volatile("mov %%rbp, %0;"
               :"=r" (bp)
               ::"memory");
  struct exec_context *current = get_current_ctx();
  struct exec_context* new = get_new_ctx();
  new->type = current->type;
  new->used_mem = current->used_mem;
  new->pgd = current->pgd;
  new->pending_signal_bitmap = 0x0;

  for(int i=0;i<MAX_SIGNALS;i++){
    new->sighandlers[i] = current->sighandlers[i];
  }
  new->ticks_to_sleep = 0;
  new->alarm_config_time = 0;
  new->ticks_to_alarm = 0;
  for(int i=0;i<MAX_MM_SEGS;i++){
    new->mms[i] = current->mms[i];
  }
  char str[3];
  int pid = new->pid;
  if(pid < 10){
    str[0] = (char)('0' + pid);
    str[1] = '\0';
  }
  else{
    str[0] = (char)('0' + pid/10);
    str[1] = (char)('0' + pid%10);
    str[2] = '\0';
  }
  char name[CNAME_MAX];
  char c = current->name[0];
  int i = 0;
  while(c!='\0'){
    name[i++] = c;
    c = current->name[i];
  }
  name[i++] = str[0];
  if(str[1]=='\0'){
    name[i++] = '\0';
  }
  else{
    name[i++] = str[1];
    name[i++] = '\0';
  }

  for(i=0;i<CNAME_MAX;i++){
    new->name[i] = name[i];
  }
  u64 ** old_bp = (u64 **)(bp);
  new->os_stack_pfn = os_pfn_alloc(OS_PT_REG);
  new->os_rsp = (u64)osmap(new->os_stack_pfn);
  new->regs = current->regs;
  new->regs.entry_rflags = *(*old_bp + 18);
  new->regs.entry_ss = 0x2b;
  new->regs.entry_cs = 0x23;
  new->regs.entry_rsp = (u64)(user_stack);
  new->regs.entry_rip = (u64)th_func;
  new->regs.rbp = new->regs.entry_rsp;
  new->state = READY;
  return new->pid;
  /*You may need to invoke the scheduler from here if there are
    other processes except swapper in the system. Make sure you make 
    the status of the current process to UNUSED before scheduling 
    the next process. If the only process alive in system is swapper, 
    invoke do_cleanup() to shutdown gem5 (by crashing it, huh!)
    */
    do_cleanup();  /*Call this conditionally, see comments above*/
}

/*system call handler for sleep*/
long do_sleep(u32 ticks)
{
   
}

/*
  system call handler for clone, create thread like 
  execution contexts
*/
long do_clone(void *th_func, void *user_stack)
{
   
}

long invoke_sync_signal(int signo, u64 *ustackp, u64 *urip)
{
  /*If signal handler is registered, manipulate user stack and RIP to execute signal handler*/
  /*ustackp and urip are pointers to user RSP and user RIP in the exception/interrupt stack*/
  printf("Called signal with ustackp=%x urip=%x\n", *ustackp, *urip);
  struct exec_context *current = get_current_ctx();
  if(current->sighandlers[signo]){
    u64** pt = (u64 **)ustackp;
    *(*pt - 1) = *urip;
    *pt = *pt - 1;
    *urip = (u64)current->sighandlers[signo];
    return 0;
  }
  /*Default behavior is exit() if sighandler is not registered for SIGFPE or SIGSEGV.
  Ignore for SIGALRM*/
  if(signo != SIGALRM)
    do_exit();
  return 0;
   /*If signal handler is registered, manipulate user stack and RIP to execute signal handler*/
   /*ustackp and urip are pointers to user RSP and user RIP in the exception/interrupt stack*/
   printf("Called signal with ustackp=%x urip=%x\n", *ustackp, *urip);
   /*Default behavior is exit( ) if sighandler is not registered for SIGFPE or SIGSEGV.
    Ignore for SIGALRM*/
    if(signo != SIGALRM)
      do_exit();
}
/*system call handler for signal, to register a handler*/
long do_signal(int signo, unsigned long handler)
{
  if(signo<0 || signo >= MAX_SIGNALS)do_exit();
  struct exec_context *current = get_current_ctx();
  long prev = (long)current->sighandlers[signo];
  current->sighandlers[signo] = (void *)handler;
  return prev;
   
}

/*system call handler for alarm*/
long do_alarm(u32 ticks)
{
  struct exec_context *current = get_current_ctx();
  u32 prev = current->ticks_to_alarm;
  current->alarm_config_time = ticks;
  current->ticks_to_alarm = ticks;
  set_bit(current->pending_signal_bitmap,SIGALRM);
  if(ticks==0)clear_bit(current->pending_signal_bitmap,SIGALRM);
  return prev;

}
