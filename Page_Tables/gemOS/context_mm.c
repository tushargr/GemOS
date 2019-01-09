#include<context.h>
#include<memory.h>
#include<lib.h>


u64 pfn_mask = 0xFFFFFFFFFF000;
u64 *addr;
u32 code_pfn;
u32 flag;
u32 page_pfn[10];
u32 num_page_pfn = 0;
u32 data_pfn[3];
u32 num_data_pfn = 0;

void fun(){
    if(flag == 0)*(addr) = (code_pfn<<12)|(0x05);
    else *(addr) = (code_pfn<<12)|(0x07);
    page_pfn[num_page_pfn++] = code_pfn;
    addr = osmap(code_pfn);
    for(u64 i=0;i<512;i++){
        u64 *cur = (addr + i);
        *cur = 0;
    }
}

void function(){
    if(((*(addr)) & 0x01)==0){
        u32 pfn = os_pfn_alloc(OS_PT_REG);
        *(addr) = 0;
        if(flag == 0)*(addr) = (pfn<<12)|(0x05);
        else *(addr) = (pfn<<12)|(0x07);
        page_pfn[num_page_pfn++] = pfn;
        addr = osmap(pfn);
        for(u64 i=0;i<512;i++){
            u64 *cur = (addr + i);
            *cur = 0;
        }
    }
    else{
        if(flag != 0){
            u64 temp = *(addr);
            *(addr) = temp|(0x07);
        }
        u32 pfn = ((*(addr))&pfn_mask)>>12;
        addr = osmap(pfn);
    }
}
void prepare_context_mm(struct exec_context *ctx)
{
    ctx->pgd = os_pfn_alloc(OS_PT_REG);
    page_pfn[num_page_pfn++] = ctx->pgd;
    u64 *l4_start = osmap(ctx->pgd);
    for(u64 i=0;i<512;i++){
        u64 *cur = l4_start + i;
        *cur = 0;
    }

    u64 l4_mask = 0x0000FF8000000000;
    u64 l3_mask = 0x0000007FC0000000;
    u64 l2_mask = 0x000000003FE00000;
    u64 l1_mask = 0x00000000001FF000;
    u64 offset_mask = 0x0000000000000FFF;
    
    u64 add_code = ctx->mms[MM_SEG_CODE].start;
    u64 code_l4 = (add_code & (l4_mask))>>39;
    u64 code_l3 = (add_code & (l3_mask))>>30;
    u64 code_l2 = (add_code & (l2_mask))>>21;
    u64 code_l1 = (add_code & (l1_mask))>>12;

    u64 add_stack = (ctx->mms[MM_SEG_STACK].end-4096);
    u64 stack_l4 = (add_stack & (l4_mask))>>39;
    u64 stack_l3 = (add_stack & (l3_mask))>>30;
    u64 stack_l2 = (add_stack & (l2_mask))>>21;
    u64 stack_l1 = (add_stack & (l1_mask))>>12;

    u64 add_data = ctx->mms[MM_SEG_DATA].start;
    u64 data_l4 = (add_data & (l4_mask))>>39;
    u64 data_l3 = (add_data & (l3_mask))>>30;
    u64 data_l2 = (add_data & (l2_mask))>>21;
    u64 data_l1 = (add_data & (l1_mask))>>12;

    u32 stack_flag = ctx->mms[MM_SEG_STACK].access_flags & 0x2;
    u32 data_flag = ctx->mms[MM_SEG_DATA].access_flags & 0x2;
    u32 code_flag = ctx->mms[MM_SEG_CODE].access_flags & 0x2;

    flag = code_flag;
    code_pfn = os_pfn_alloc(OS_PT_REG);
    addr = l4_start + code_l4;
    fun();
    code_pfn = os_pfn_alloc(OS_PT_REG);
    addr = addr + code_l3;
    fun();
    code_pfn = os_pfn_alloc(OS_PT_REG);
    addr = addr + code_l2;
    fun();
    code_pfn = os_pfn_alloc(USER_REG);
    addr = addr + code_l1;
    if(code_flag==0) *(addr) = ((code_pfn<<12)|(0x05));
    else *(addr) = ((code_pfn<<12)|(0x07));
    data_pfn[num_data_pfn++] = code_pfn;

    flag = stack_flag;
    addr = l4_start + stack_l4;
    function();
    addr = addr + stack_l3;
    function();
    addr = addr + stack_l2;
    function();
    addr = addr + stack_l1;
    if(((*(addr)) & 0x01)==0){
        u32 pfn = os_pfn_alloc(USER_REG);
        if(stack_flag==0) *(addr) = ((pfn<<12)|(0x05));
        else *(addr) = ((pfn<<12)|(0x07));
        data_pfn[num_data_pfn++] = pfn;
        addr = osmap(pfn);
    }

    flag = data_flag;
    addr = l4_start + data_l4;
    function();
    addr = addr + data_l3;
    function();
    addr = addr + data_l2;
    function();
    addr = addr + data_l1;
    if((*(addr) & 0x1)==0){
        u32 pfn = ctx->arg_pfn;
        if(data_flag==0) *(addr) = ((pfn<<12)|(0x05));
        else *(addr) = ((pfn<<12)|(0x07));
        addr = osmap(pfn);
    }
    data_pfn[num_data_pfn++] = ctx->arg_pfn;
    return;
}
void cleanup_context_mm(struct exec_context *ctx)
{
    for(u32 i=0;i<num_page_pfn;i++){
        os_pfn_free(OS_PT_REG,page_pfn[i]);
    }
    num_page_pfn = 0;
    for(u32 i=0;i<num_data_pfn;i++){
        os_pfn_free(USER_REG,data_pfn[i]);
    }
    num_data_pfn=0;
    return;
}
