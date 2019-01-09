#include<init.h>
#include<lib.h>
#include<memory.h>
#include<context.h>

#define LVL_MASK ((u64) ((1ULL<<9) - 1))
#define NXT_LVL_MASK ((u64) ((1ULL<<40) - 1))

int check_valid_memory(struct exec_context* ctx, void* buffer) {
    void* page_l4 = osmap((u64)ctx->pgd);
    u64 buf_addr = (u64)buffer;
    void* page_p = page_l4;

    for (int i = 3; i >= 0; i--) {
        u64 d_l = (buf_addr>>(12 + 9*i)) & LVL_MASK;
        u64 pfn_n = (*((u64*)page_p + d_l) >> 12) & NXT_LVL_MASK;
        u64* pte = (u64*)page_p + d_l;
        if ((*pte & 1ULL) == 0ULL || (*pte & 4ULL) == 0ULL) {
            return 0;
        }
        page_p = osmap(pfn_n);
    }
    return 1;
}

void release_pfn(struct exec_context* ctx, u64 addr) {
    void* page_l4 = osmap((u64)ctx->pgd);
    void* page_p = page_l4;
    for (int i = 3; i >= 0; i--) {
        u64 d_l = (addr>>(12 + 9*i)) & LVL_MASK;
        u64 pfn_n = (*((u64*)page_p + d_l) >> 12) & NXT_LVL_MASK;
        u64* pte = (u64*)page_p + d_l;
        if (i == 0 && (*pte & 1ULL) != 0ULL && (*pte & 4ULL) != 0) {
            os_pfn_free(USER_REG, pfn_n);
            *pte = 0ULL;
        } else {
            page_p = osmap(pfn_n);
        }
    }
    return;
}

/*System Call handler*/
u64 do_syscall(int syscall, u64 param1, u64 param2, u64 param3, u64 param4)
{
    struct exec_context *current = get_current_ctx();
    printf("[GemOS] System call invoked. syscall no  = %d\n", syscall);
    switch(syscall)
    {
          case SYSCALL_EXIT:
                              printf("[GemOS] exit code = %d\n", (int) param1);
                              do_exit();
                              break;
          case SYSCALL_GETPID:
                              printf("[GemOS] getpid called for process %s, with pid = %d\n", current->name, current->id);
                              return current->id;     
          case SYSCALL_WRITE:
                             {  
                                 static int a = 1;
                                 a += 1;
                                 printf("(%d) %d %x\n", 1, a, (u64)(&a));
                                 char* buffer = (char*) param1;
                                 int length = (int) param2;
                                 if (length > 1024 || length < 0) {
                                     return -1;
                                 }
                                 if (check_valid_memory(current, (void*)buffer) == 1 &&
                                     check_valid_memory(current, (void*)buffer + length - 1) == 1) {
                                     for (int i = 0; i < length; i++) {
                                         printf("%c", buffer[i]);
                                     }
                                 } else {
                                     return -1;
                                 }
                                 return 0;
                             }
          case SYSCALL_EXPAND:
                             {  
                                 u32 size = (u32) param1;
                                 int flags = (int) param2;
                                 if (size > 512 || (flags != MAP_RD && flags != MAP_WR)) {
                                     printf("[GemOS] illegal syscall done.\n");
                                     return 0;
                                 }
                                 u64 MM_SEG;
                                 if (flags == MAP_WR) {
                                     MM_SEG = MM_SEG_DATA;
                                 } else {
                                     MM_SEG = MM_SEG_RODATA;
                                 }
                                 struct mm_segment* mm_flag = current->mms + MM_SEG;
                                 if (mm_flag->next_free + (size<<12) > mm_flag->end) {
                                     return 0;
                                 }
                                 u64 ret = mm_flag->next_free;
                                 mm_flag->next_free = mm_flag->next_free + (size<<12);
                                 return ret;
                             }
          case SYSCALL_SHRINK:
                             {  
                                 u32 size = (u32) param1;
                                 int flags = (int) param2;
                                 if (flags != MAP_RD && flags != MAP_WR) {
                                     printf("[GemOS] illegal syscall done.\n");
                                     return 0;
                                 }
                                 u64 MM_SEG;
                                 if (flags == MAP_WR) {
                                     MM_SEG = MM_SEG_DATA;
                                 } else {
                                     MM_SEG = MM_SEG_RODATA;
                                 }
                                 struct mm_segment* mm_flag = current->mms + MM_SEG;
                                 if (mm_flag->next_free < (size<<12) + mm_flag->start) {
                                     return 0;
                                 }
                                 mm_flag->next_free = mm_flag->next_free - (size<<12);
                                 for (u32 i = 0; i < size; i++) {
                                     u64 page = mm_flag->next_free + (i<<12);
                                     asm volatile(
                                             "invlpg (%0);"
                                             :
                                             :"r"(page)
                                             :"memory"
                                             );
                                     release_pfn(current, page);
                                 }
                                 return mm_flag->next_free;
                             }
          default:
                              return -1;
    }
    return 0;   /*GCC shut up!*/
}

extern int handle_div_by_zero(void)
{
    register u64 rbp asm ("rbp");
    u64* addr = (u64*)rbp + 1;
    printf("[GemOS] Div-by-zero handler: [RIP: %x]\n", *addr);
    do_exit();
    return 0;
}

void setup_page_table(u64 va, struct exec_context* ctx, u32 access_flags) {
    void* page_l4 = osmap((u64)ctx->pgd);
    void* page_p = page_l4;
    u64 wbit = (access_flags & 2U) > 0U;
    for (int i = 3; i >= 0; i--) {
        u64 d_l = (va>>(12 + 9*i)) & LVL_MASK;
        u64* pte = (u64*)page_p + d_l;
        if ((*pte & 1ULL) == 0ULL) {
            if (i == 0) {
                u64 pfn_li = (u64) os_pfn_alloc(USER_REG);
                *pte = 0ULL;
                *pte |= (pfn_li<<12);
                *pte |= 5ULL;
            } else {
                u64 pfn_li = (u64) os_pfn_alloc(OS_PT_REG);
                *pte = 0ULL;
                *pte |= (pfn_li<<12);
                *pte |= 5ULL;
            }
        }
        if (wbit) {
            *pte |= 2ULL;
        } else {
            *pte &= (~2ULL);
        }
        u64 pfn_n = ((*pte) >> 12) & NXT_LVL_MASK;
        page_p = osmap(pfn_n);
    }
    return;
}

extern int handle_page_fault(void)
{
    u64 rsp, cr2, rbp;
    asm volatile(
            "push %r8;"
            "push %r9;"
            "push %r10;"
            "push %r11;"
            "push %r12;"
            "push %r13;"
            "push %r14;"
            "push %r15;"
            "push %rax;"
            "push %rbx;"
            "push %rcx;"
            "push %rdx;"
            "push %rdi;"
            "push %rsi;"
            );
    asm volatile("mov %%rsp, %0;":"=r"(rsp));
    asm volatile("mov %%cr2, %0;":"=r"(cr2));
    asm volatile("mov %%rbp, %0;":"=r"(rbp));

    u64 va = cr2;
    u64 errorcode = *((u64*)rbp + 1);
    u64 rip = *((u64*)rbp + 2);
    u64 rip_addr = ((u64*)rbp + 2);
    struct exec_context* ctx = get_current_ctx();
    struct mm_segment* mm_data = ctx->mms + MM_SEG_DATA;
    struct mm_segment* mm_rodata = ctx->mms + MM_SEG_RODATA;
    struct mm_segment* mm_stack = ctx->mms + MM_SEG_STACK;
    printf("[GemOS] page fault handler: [RIP: %x] [VA: %lx] [Error: %x]\n", rip, va, errorcode);

    if (mm_data->start <= va && va < mm_data->next_free) {
        setup_page_table(va, ctx, mm_data->access_flags);
    } else if (mm_rodata->start <= va && va < mm_rodata->next_free) {
        if ((errorcode & 2) != 0) {
            printf("[GemOS] page fault handle: Write access in read only region [RIP: %x] [VA: %x] [Error: %x]\n", rip, va, errorcode);
            do_exit();
        }
        setup_page_table(va, ctx, mm_rodata->access_flags);
    } else if (mm_stack->start <= va && va < mm_stack->end) {
        setup_page_table(va, ctx, mm_stack->access_flags);
    } else {
        printf("[GemOS] page fault handle: Invalid memory region [RIP: %x] [VA: %x] [Error: %x]\n", rip, va, errorcode);
        do_exit();
    }

    asm volatile("mov %0, %%rsp;"
            :
            :"r"(rsp)
            : "memory");
    asm volatile(
            "pop %rsi;"
            "pop %rdi;"
            "pop %rdx;"
            "pop %rcx;"
            "pop %rbx;"
            "pop %rax;"
            "pop %r15;"
            "pop %r14;"
            "pop %r13;"
            "pop %r12;"
            "pop %r11;"
            "pop %r10;"
            "pop %r9;"
            "pop %r8;"
            );
    asm volatile(
            "mov %%rbp, %%rsp;"
            "pop %%rbp;"
            "add $8, %%rsp;"
            :
            :
            : "memory");
    asm volatile("iretq;");
    return 0;
}
