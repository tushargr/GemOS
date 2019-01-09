#include<init.h>
#include<lib.h>
#include<context.h>
#include<memory.h>

u64 check_legitimate(u64 param1,u32 pgd, u32 flag){
    u64 l4_mask = 0x0FF8000000000;
    u64 l3_mask = 0x07FC0000000;
    u64 l2_mask = 0x03FE00000;
    u64 l1_mask = 0x01FF000;
    u64 pfn_mask = 0xFFFFFFFFFF000;
    u64 ind_l4 = (param1 & (l4_mask))>>39;
    u64 ind_l3 = (param1 & (l3_mask))>>30;
    u64 ind_l2 = (param1 & (l2_mask))>>21;
    u64 ind_l1 = (param1 & (l1_mask))>>12;
    u32 l4_pfn = pgd;
    u64 *l4_addr = (u64 *)osmap(l4_pfn);
    u64 pte_l4 = *(l4_addr+ind_l4);
    if((pte_l4 & 0x1)==0){
	return 0;
    }
    u32 l3_pfn = (pte_l4 & pfn_mask)>>12;
    u64 *l3_addr = (u64 *)osmap(l3_pfn);
    u64 pte_l3 = *(l3_addr+ind_l3);
    if((pte_l3 & 0x1)==0){
	return 0;
    }
    u32 l2_pfn = (pte_l3 & pfn_mask)>>12;
    u64 *l2_addr = (u64 *)osmap(l2_pfn);
    u64 pte_l2 = *(l2_addr+ind_l2);
    if((pte_l2 & 0x1)==0){
         return 0;
    }
    u32 l1_pfn = (pte_l2 & pfn_mask)>>12;
    u64 *l1_addr = (u64 *)osmap(l1_pfn);
    u64 pte_l1 = *(l1_addr+ind_l1);
    if(((pte_l1 & 0x5)!=0x5)){
	return 0;
    }
    return pte_l1;
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
                                u64 pfn_mask = 0xFFFFFFFFFF000;
                                u64 offset_mask = 0x0FFF;
                                u64 pte;
                                u32 pgd = current->pgd;
                                if(param2>0x400 || param2 <0){
                                    return -1;
                                }
                                if(param2 == 0)return 0;
                                pte = check_legitimate(param1,pgd,0);
				                if(pte == 0)return -1;
                                u64 temp = check_legitimate(param1+param2-1,pgd,1);
                				if(temp == 0)return -1;
                				u32 mem_pfn = (pte & pfn_mask)>>12;
								if(pte == 0)return -1;
                                u64 temp = check_legitimate(param1+param2-1,pgd,1);
								if(temp == 0)return -1;
								u32 mem_pfn = (pte & pfn_mask)>>12;
                                u64 *mem_addr = (u64 *)osmap(mem_pfn);
                                u64 mem_ind = (param1 & (offset_mask));
                                u64* val = (u64 *)((char *)mem_addr+mem_ind);
                                for(int i=0;i<param2;i++){
                                    printf("%c",*((char *)(param1)+i));
                                }
                                return param2;
                             }
          case SYSCALL_EXPAND:
                             {
                                if(param1>512 || param1<0)return 0;
                                param1 = param1*4096;
                                if(param2 == MAP_RD){
                                    u64 end = current->mms[MM_SEG_RODATA].end;
                                    u64 next_free = current->mms[MM_SEG_RODATA].next_free;
                                    u64 temp = next_free;
                                    if((next_free+param1)<=(end+1)){
                                        current->mms[MM_SEG_RODATA].next_free = (next_free+param1);
                                    }
                                    else{
                                        return 0;
                                    }
                                    return (temp);
                                }
                                if(param2 == MAP_WR){
                                    u64 end = current->mms[MM_SEG_DATA].end;
                                    u64 next_free = current->mms[MM_SEG_DATA].next_free;
                                    u64 temp = next_free;
                                    if((next_free+param1)<=(end+1)){
                                        current->mms[MM_SEG_DATA].next_free = (next_free+param1);
                                    }
                                    else{
                                        return 0;
                                    }
                                    return (temp);
                                }
                             }
          case SYSCALL_SHRINK:
                             {
                                u64 l4_mask = 0x0FF8000000000;
                                u64 l3_mask = 0x07FC0000000;
                                u64 l2_mask = 0x03FE00000;
                                u64 l1_mask = 0x01FF000;
                                u64 pfn_mask = 0xFFFFFFFFFF000;
                                param1 = param1*4096;
                                u64 start, next_free;
                                u64 temp;
                                if(param2 == MAP_RD){
                                    start = current->mms[MM_SEG_RODATA].start;
                                    next_free = current->mms[MM_SEG_RODATA].next_free;
                                    if((next_free-param1)<start){
                                        return 0;
                                    }
                                    temp  = (next_free - param1);
                                    u32 l4_pfn = current->pgd;
                                    u64 *l4_addr = (u64 *)osmap(l4_pfn);
                                    u32 i=0;
                                    for(u64 vir = (current->mms[MM_SEG_RODATA].next_free-0x1000);i<(param1/4096);i++){
                                        u64 pte_l4 = *(l4_addr+((vir & (l4_mask))>>39));
                                        //(*(l4_addr+((vir & (l4_mask))>>39))) = 0x0;
                                        if((pte_l4 & 0x1)==0){
                                            vir -= 0x1000;
                                            continue;
                                        }
                                        u32 l3_pfn = (pte_l4 & pfn_mask)>>12;
                                        u64 *l3_addr = (u64 *)osmap(l3_pfn);
                                        u64 pte_l3 = *(l3_addr+((vir & (l3_mask))>>30));
                                        //(*(l3_addr+((vir & (l3_mask))>>30))) = 0x0;
                                        if((pte_l3 & 0x1)==0){
                                            vir -= 0x1000;
                                            continue;
                                        }
                                        u32 l2_pfn = (pte_l3 & pfn_mask)>>12;
                                        u64 *l2_addr = (u64 *)osmap(l2_pfn);
                                        u64 pte_l2 = *(l2_addr+((vir & (l2_mask))>>21));
                                        //(*(l2_addr+((vir & (l2_mask))>>21))) = 0x0;
                                        if((pte_l2 & 0x1)==0){
                                            vir -= 0x1000;
                                            continue;
                                        }
                                        u32 l1_pfn = (pte_l2 & pfn_mask)>>12;
                                        u64 *l1_addr = (u64 *)osmap(l1_pfn);
                                        u64 pte_l1 = *(l1_addr+((vir & (l1_mask))>>12));
                                        if((pte_l1 & 0x1)==0){
                                            vir -= 0x1000;
                                            continue;
                                        }
                                        u32 pfn = ((*(l1_addr+((vir & (l1_mask))>>12)))&0xFFFFFFFF);
                                        os_pfn_free(USER_REG,pfn);
                                        *(l1_addr+((vir & (l1_mask))>>12)) = 0x0;
                                        asm volatile("invlpg (%0)" ::"r" (vir) : "memory");
                                        vir -= 0x1000;
                                    }
                                    current->mms[MM_SEG_RODATA].next_free -= param1;
                                    return (temp);
                                }
                                else if(param2 == MAP_WR){
                                    start = current->mms[MM_SEG_DATA].start;
                                    next_free = current->mms[MM_SEG_DATA].next_free;
                                    if((next_free-param1)<start){
                                        return 0;
                                    }
                                    temp  = (next_free - param1);
                                    u32 l4_pfn = current->pgd;
                                    u64 *l4_addr = (u64 *)osmap(l4_pfn);
                                    u32 i=0;
                                    for(u64 vir = (current->mms[MM_SEG_DATA].next_free-0x1000);i<(param1/4096);i++){
                                        u64 pte_l4 = *(l4_addr+((vir & (l4_mask))>>39));
                                        if((pte_l4 & 0x1)==0x0){
                                            vir -= 0x1000;
                                            continue;
                                        }
                                        u32 l3_pfn = (pte_l4 & pfn_mask)>>12;
                                        u64 *l3_addr = (u64 *)osmap(l3_pfn);
                                        u64 pte_l3 = *(l3_addr+((vir & (l3_mask))>>30));
                                        if((pte_l3 & 0x1)==0x0){
                                            vir -= 0x1000;
                                            continue;
                                        }
                                        u32 l2_pfn = (pte_l3 & pfn_mask)>>12;
                                        u64 *l2_addr = (u64 *)osmap(l2_pfn);
                                        u64 pte_l2 = *(l2_addr+((vir & (l2_mask))>>21));
                                        if((pte_l2 & 0x1)==0x0){
                                            vir -= 0x1000;
                                            continue;
                                        }
                                        u32 l1_pfn = (pte_l2 & pfn_mask)>>12;
                                        u64 *l1_addr = (u64 *)osmap(l1_pfn);
                                        u64 pte_l1 = *(l1_addr+((vir & (l1_mask))>>12));
                                        if((pte_l1 & 0x1)==0x0){
                                            vir -= 0x1000;
                                            continue;
                                        }
                                        u32 pfn = (pte_l1 & pfn_mask)>>12;
                                        *(l1_addr+((vir & (l1_mask))>>12)) = 0x0;
                                        pte_l1 = *(l1_addr+((vir & (l1_mask))>>12));
                                        os_pfn_free(USER_REG,pfn);
                                        asm volatile("invlpg (%0)" ::"r" (vir) : "memory");
                                        vir -= 0x1000;
                                    }
                                    current->mms[MM_SEG_DATA].next_free -= param1;
                                    return (temp);
                                }
                             }
                             
          default:
                              return -1;
                                
    }
    return 0;   /*GCC shut up!*/
}

extern int handle_div_by_zero(void)
{
    u64 * bp;
    asm volatile("mov %%rbp, %0;"
                  :"=r" (bp)
                  :
                  : "memory");
    printf("Div-by-zero detected at [%x]\n",*(bp+1));
    do_exit();
    return 0;
}

u64* allocate(u64* addr,u32 flag,u64 error_code){
    u64 pfn_mask = 0xFFFFFFFFFF000;   
    if(((*(addr)) & 0x1)==0){
        u32 pfn = os_pfn_alloc(OS_PT_REG);
        *(addr) = ((pfn<<12)|0x05|(flag & 0x2));
        addr = (u64 *)osmap(pfn);
        for(u64 i=0;i<512;i++){
            *(addr+i) = 0;
        }
        return addr;
    }
    else{
        if(flag == 2){
            u64 temp = *(addr);
            *(addr) = (temp|0x07);
        }
        u32 pfn = ((*(addr))&pfn_mask)>>12;
        addr = (u64 *)osmap(pfn);
        return addr;
    }
}

extern int handle_page_fault(void)
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

    u64 bp,vir;

    asm volatile("mov %%rbp, %0;"
                  :"=r" (bp)
                  :
                  : "memory");
    asm volatile("mov %%cr2,%0":"=r"(vir));
    u64 rip,error_code,newsp;
    u64 pgd = get_current_ctx()->pgd;
    u64 * virtual;
    u64 * start = (u64 *)osmap(pgd);
    u32 write = 0;
    u64 l4_mask = 0x0FF8000000000;
    u64 l3_mask = 0x07FC0000000;
    u64 l2_mask = 0x03FE00000;
    u64 l1_mask = 0x01FF000;
    
    newsp = bp+16;
    pgd = get_current_ctx()->pgd;
    start = (u64 *)osmap(pgd);
    error_code = (*((u64 *)(bp+8)));
    rip = (*((u64 *)(bp+16)));

    if((error_code & 0x1) == 0x1){
        printf("[Protection Violation] RIP = %x Accessed address = %x and Error Code = %x\n",*((u64 *)(bp+16)),vir,*((u64 *)(bp+8)));
        do_exit();
    }
    if((vir>=get_current_ctx()->mms[MM_SEG_DATA].start) && (vir<get_current_ctx()->mms[MM_SEG_DATA].next_free)) write = 2;
    else{
        if((vir>=get_current_ctx()->mms[MM_SEG_RODATA].start) && (vir<get_current_ctx()->mms[MM_SEG_RODATA].next_free) && ((error_code & 0x2) == 0x0))write = 0;
        else{
            if(vir>=get_current_ctx()->mms[MM_SEG_STACK].start && vir<get_current_ctx()->mms[MM_SEG_STACK].end) write = 2;
            else{
                printf("Illegitimate address accesed with RIP = %x Accessed address = %x and Error Code = %x\n",rip,vir,error_code);
                do_exit();
            }
        }
    }
    virtual = start + ((vir & (l4_mask))>>39);
    virtual = allocate(virtual,write,error_code);
    virtual = virtual + ((vir & (l3_mask))>>30);
    virtual = allocate(virtual,write,error_code);
    virtual = virtual + ((vir & (l2_mask))>>21);
    virtual = allocate(virtual,write,error_code);
    virtual = virtual + ((vir & (l1_mask))>>12);
    if(((*(virtual)) & 0x01)==0x0){
        u32 pfn = os_pfn_alloc(USER_REG);
        *(virtual) = ((pfn<<12)|0x05|(write & 0x2));
    }
    asm volatile(
            "mov %0, %%rsp;"
            "pop %%r15;""pop %%r14;""pop %%r13;""pop %%r12;"
            "pop %%r11;""pop %%r10;""pop %%r9;""pop %%r8;"
            "pop %%rdi;""pop %%rsi;""pop %%rdx;""pop %%rcx;"
            "pop %%rbx;""pop %%rax;"
            "mov %%rbp, %%rsp;"
            "pop %%rbp;"
            "add $8, %%rsp;"
            "iretq;"
            :
            : "r" (pop_top)
            : "memory");
    return 0;
}
