/******************************************************************************
 * dmpatch.c  ---- patch for device-mapper
 *
 * Copyright (c) 2021, longpanda <admin@ventoy.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/mutex.h>
#include <linux/mempool.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/slab.h>

#define MAX_PATCH   4

#define magic_sig 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF

typedef int (*kprobe_reg_pf)(void *);
typedef void (*kprobe_unreg_pf)(void *);
typedef int (*printk_pf)(const char *fmt, ...);
typedef int (*set_memory_attr_pf)(unsigned long addr, int numpages);

#pragma pack(1)
typedef struct ko_param
{
    unsigned char magic[16];
    unsigned long struct_size;
    unsigned long pgsize;
    unsigned long printk_addr;
    unsigned long ro_addr;
    unsigned long rw_addr;
    unsigned long reg_kprobe_addr;
    unsigned long unreg_kprobe_addr;
    unsigned long sym_get_addr;
    unsigned long sym_get_size;
    unsigned long sym_put_addr;
    unsigned long sym_put_size;
    unsigned long kv_major;
    unsigned long ibt;
    unsigned long kv_minor;
    unsigned long blkdev_get_addr;
    unsigned long blkdev_put_addr;
    unsigned long bdev_open_addr;
    unsigned long kv_subminor;
    unsigned long bdev_file_open_addr;
    unsigned long padding[1];
}ko_param;

#pragma pack()

static printk_pf kprintf = NULL;
static set_memory_attr_pf set_mem_ro = NULL;
static set_memory_attr_pf set_mem_rw = NULL;
static kprobe_reg_pf reg_kprobe = NULL;
static kprobe_unreg_pf unreg_kprobe = NULL;

static volatile ko_param g_ko_param = 
{
    { magic_sig },
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

#if defined(CONFIG_X86_64)
#define PATCH_OP_POS1    3
#define CODE_MATCH1(code, i) \
    (code[i] == 0x40 && code[i + 1] == 0x80 && code[i + 2] == 0xce && code[i + 3] == 0x80)

#define PATCH_OP_POS2    1
#define CODE_MATCH2(code, i) \
    (code[i] == 0x0C && code[i + 1] == 0x80 && code[i + 2] == 0x89 && code[i + 3] == 0xC6)
    
#define PATCH_OP_POS3    4
#define CODE_MATCH3(code, i) \
    (code[i] == 0x44 && code[i + 1] == 0x89 && code[i + 2] == 0xe8 && code[i + 3] == 0x0c && code[i + 4] == 0x80)





#elif defined(CONFIG_X86_32)
#define PATCH_OP_POS1    2
#define CODE_MATCH1(code, i) \
    (code[i] == 0x80 && code[i + 1] == 0xca && code[i + 2] == 0x80 && code[i + 3] == 0xe8)

#define PATCH_OP_POS2    PATCH_OP_POS1
#define CODE_MATCH2      CODE_MATCH1
#define PATCH_OP_POS3    PATCH_OP_POS1
#define CODE_MATCH3      CODE_MATCH1


#else
#error "unsupported arch"
#endif

#ifdef VTOY_IBT
#ifdef CONFIG_X86_64
/* Using 64-bit values saves one instruction clearing the high half of low */
#define DECLARE_ARGS(val, low, high)	unsigned long low, high
#define EAX_EDX_VAL(val, low, high)	((low) | (high) << 32)
#define EAX_EDX_RET(val, low, high)	"=a" (low), "=d" (high)
#else
#define DECLARE_ARGS(val, low, high)	unsigned long long val
#define EAX_EDX_VAL(val, low, high)	(val)
#define EAX_EDX_RET(val, low, high)	"=A" (val)
#endif

#define	EX_TYPE_WRMSR			 8
#define	EX_TYPE_RDMSR			 9
#define MSR_IA32_S_CET			0x000006a2 /* kernel mode cet */
#define CET_ENDBR_EN			(1ULL << 2)

/* Exception table entry */
#ifdef __ASSEMBLY__

#define _ASM_EXTABLE_TYPE(from, to, type)			\
	.pushsection "__ex_table","a" ;				\
	.balign 4 ;						\
	.long (from) - . ;					\
	.long (to) - . ;					\
	.long type ;						\
	.popsection

#else /* ! __ASSEMBLY__ */

#define _ASM_EXTABLE_TYPE(from, to, type)			\
	" .pushsection \"__ex_table\",\"a\"\n"			\
	" .balign 4\n"						\
	" .long (" #from ") - .\n"				\
	" .long (" #to ") - .\n"				\
	" .long " __stringify(type) " \n"			\
	" .popsection\n"

#endif /* __ASSEMBLY__ */
#endif /* VTOY_IBT */






#define vdebug(fmt, args...) if(kprintf) kprintf(KERN_ERR fmt, ##args)

static unsigned int g_claim_ptr = 0;
static unsigned char *g_get_patch[MAX_PATCH] = { NULL };
static unsigned char *g_put_patch[MAX_PATCH] = { NULL };

static int notrace dmpatch_kv_above(unsigned long Major, unsigned long Minor, unsigned long SubMinor)
{
    if (g_ko_param.kv_major != Major)
    {
        return (g_ko_param.kv_major > Major) ? 1 : 0;
    }

    if (g_ko_param.kv_minor != Minor)
    {
        return (g_ko_param.kv_minor > Minor) ? 1 : 0;
    }

    if (g_ko_param.kv_subminor != SubMinor)
    {
        return (g_ko_param.kv_subminor > SubMinor) ? 1 : 0;
    }

    return 1;
}

static void notrace dmpatch_restore_code(int bytes, unsigned char *opCode, unsigned int code)
{
    unsigned long align;

    if (opCode)
    {
        align = (unsigned long)opCode / g_ko_param.pgsize * g_ko_param.pgsize;
        set_mem_rw(align, 1);
        if (bytes == 1)
        {
            *opCode = (unsigned char)code;            
        }
        else
        {
            *(unsigned int *)opCode = code;
        }
        set_mem_ro(align, 1);        
    }
}

static int notrace dmpatch_replace_code
(
    int style,
    unsigned long addr, 
    unsigned long size, 
    int expect, 
    const char *desc, 
    unsigned char **patch
)
{
    int i = 0;
    int cnt = 0;
    unsigned long align;
    unsigned char *opCode = (unsigned char *)addr;

    vdebug("patch for %s style[%d] 0x%lx %d\n", desc, style, addr, (int)size);

    for (i = 0; i < (int)size - 8; i++)
    {
        if (style == 1)
        {
            if (CODE_MATCH1(opCode, i) && cnt < MAX_PATCH)
            {
                patch[cnt] = opCode + i + PATCH_OP_POS1;
                cnt++;
            }
        }
        else if (style == 2)
        {
            if (CODE_MATCH2(opCode, i) && cnt < MAX_PATCH)
            {
                patch[cnt] = opCode + i + PATCH_OP_POS2;
                cnt++;
            }
        }
        else if (style == 3)
        {
            if (CODE_MATCH3(opCode, i) && cnt < MAX_PATCH)
            {
                patch[cnt] = opCode + i + PATCH_OP_POS3;
                cnt++;
            }
        }
    }





    if (cnt != expect || cnt >= MAX_PATCH)
    {
        vdebug("patch error: cnt=%d expect=%d\n", cnt, expect);
        return 1;
    }


    for (i = 0; i < cnt; i++)
    {
        opCode = patch[i];
        align = (unsigned long)opCode / g_ko_param.pgsize * g_ko_param.pgsize;

        set_mem_rw(align, 1);
        *opCode = 0;
        set_mem_ro(align, 1);
    }

    return 0;
}

static unsigned long notrace dmpatch_find_call_offset(unsigned long addr, unsigned long size, unsigned long func)
{
    unsigned long i = 0;
    unsigned long dest;
    unsigned char *opCode = NULL;
    unsigned char aucOffset[8] = { 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF };

    opCode = (unsigned char *)addr;
    
    for (i = 0; i + 4 < size; i++)
    {
        if (opCode[i] == 0xE8)
        {
            aucOffset[0] = opCode[i + 1];
            aucOffset[1] = opCode[i + 2];
            aucOffset[2] = opCode[i + 3];
            aucOffset[3] = opCode[i + 4];

            dest = addr + i + 5 + *(unsigned long *)aucOffset;
            if (dest == func)
            {
                return i;
            }
        }
    }

    return 0;
}

static unsigned int notrace dmpatch_patch_claim_ptr(void)
{
    unsigned long i = 0;
    unsigned long t = 0;
    unsigned long offset1 = 0;
    unsigned long offset2 = 0;
    unsigned long align = 0;
    unsigned char *opCode = NULL;

    opCode = (unsigned char *)g_ko_param.sym_get_addr;
    for (i = 0; i < 4; i++)
    {
        vdebug("%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
            opCode[i + 0], opCode[i + 1], opCode[i + 2], opCode[i + 3],
            opCode[i + 4], opCode[i + 5], opCode[i + 6], opCode[i + 7],
            opCode[i + 8], opCode[i + 9], opCode[i + 10], opCode[i + 11],
            opCode[i + 12], opCode[i + 13], opCode[i + 14], opCode[i + 15]);
    }

    if (dmpatch_kv_above(6, 7, 0)) /* >= 6.7 kernel */
    {
        vdebug("Get addr: 0x%lx %lu open 0x%lx\n", g_ko_param.sym_get_addr, g_ko_param.sym_get_size, g_ko_param.bdev_open_addr);
        offset1 = dmpatch_find_call_offset(g_ko_param.sym_get_addr, g_ko_param.sym_get_size, g_ko_param.bdev_open_addr);
        if (offset1 == 0)
        {
            vdebug("call bdev_open_addr Not found\n");

            vdebug("Get addr: 0x%lx %lu file_open 0x%lx\n", g_ko_param.sym_get_addr, g_ko_param.sym_get_size, g_ko_param.bdev_file_open_addr);
            offset1 = dmpatch_find_call_offset(g_ko_param.sym_get_addr, g_ko_param.sym_get_size, g_ko_param.bdev_file_open_addr);
            if (offset1 == 0)
            {            
                vdebug("call bdev_file_open_addr Not found\n");
                return 1;
            }
        }
    }
    else
    {
        vdebug("Get addr: 0x%lx %lu 0x%lx\n", g_ko_param.sym_get_addr, g_ko_param.sym_get_size, g_ko_param.blkdev_get_addr);
        vdebug("Put addr: 0x%lx %lu 0x%lx\n", g_ko_param.sym_put_addr, g_ko_param.sym_put_size, g_ko_param.blkdev_put_addr);        

        offset1 = dmpatch_find_call_offset(g_ko_param.sym_get_addr, g_ko_param.sym_get_size, g_ko_param.blkdev_get_addr);
        offset2 = dmpatch_find_call_offset(g_ko_param.sym_put_addr, g_ko_param.sym_put_size, g_ko_param.blkdev_put_addr);
        if (offset1 == 0 || offset2 == 0)
        {
            vdebug("call blkdev_get or blkdev_put Not found, %lu %lu\n", offset1, offset2);
            return 1;
        }
    }

    
    vdebug("call addr1:0x%lx  call addr2:0x%lx\n", 
        g_ko_param.sym_get_addr + offset1, 
        g_ko_param.sym_put_addr + offset2);
    
    opCode = (unsigned char *)g_ko_param.sym_get_addr;
    for (i = offset1 - 1, t = 0; (i > 0) && (t < 24); i--, t++)
    {
        /* rdx */
        if (opCode[i] == 0x48 && opCode[i + 1] == 0xc7 && opCode[i + 2] == 0xc2)
        {
            g_claim_ptr = *(unsigned int *)(opCode + i + 3);
            g_get_patch[0] = opCode + i + 3;
            vdebug("claim_ptr(%08X) found at get addr 0x%lx\n", g_claim_ptr, g_ko_param.sym_get_addr + i + 3);
            break;
        }
    }

    if (g_claim_ptr == 0)
    {
        vdebug("Claim_ptr not found in get\n");
        return 1;
    }

    
    align = (unsigned long)g_get_patch[0] / g_ko_param.pgsize * g_ko_param.pgsize;
    set_mem_rw(align, 1);
    *(unsigned int *)(g_get_patch[0]) = 0;
    set_mem_ro(align, 1);  


    if (offset2 > 0)
    {        
        opCode = (unsigned char *)g_ko_param.sym_put_addr;
        for (i = offset2 - 1, t = 0; (i > 0) && (t < 24); i--, t++)
        {
            /* rsi */
            if (opCode[i] == 0x48 && opCode[i + 1] == 0xc7 && opCode[i + 2] == 0xc6)
            {
                if (*(unsigned int *)(opCode + i + 3) == g_claim_ptr)
                {
                    vdebug("claim_ptr found at put addr 0x%lx\n", g_ko_param.sym_put_addr + i + 3);
                    g_put_patch[0] = opCode + i + 3;
                    break;                
                }
            }
        }    

        if (g_put_patch[0] == 0)
        {
            vdebug("Claim_ptr not found in put\n");
            return 1;
        }
        
        align = (unsigned long)g_put_patch[0] / g_ko_param.pgsize * g_ko_param.pgsize;
        set_mem_rw(align, 1);
        *(unsigned int *)(g_put_patch[0]) = 0;
        set_mem_ro(align, 1);   
    }

    return 0;
}

#ifdef VTOY_IBT
static __always_inline unsigned long long dmpatch_rdmsr(unsigned int msr)
{
	DECLARE_ARGS(val, low, high);

	asm volatile("1: rdmsr\n"
		     "2:\n"
		     _ASM_EXTABLE_TYPE(1b, 2b, EX_TYPE_RDMSR)
		     : EAX_EDX_RET(val, low, high) : "c" (msr));

	return EAX_EDX_VAL(val, low, high);
}

static __always_inline void dmpatch_wrmsr(unsigned int msr, u32 low, u32 high)
{
	asm volatile("1: wrmsr\n"
		     "2:\n"
		     _ASM_EXTABLE_TYPE(1b, 2b, EX_TYPE_WRMSR)
		     : : "c" (msr), "a"(low), "d" (high) : "memory");
}

static u64 notrace dmpatch_ibt_save(void)
{
    u64 msr = 0;
    u64 val = 0;

    msr = dmpatch_rdmsr(MSR_IA32_S_CET);
    val = msr & ~CET_ENDBR_EN;
    dmpatch_wrmsr(MSR_IA32_S_CET, (u32)(val & 0xffffffffULL), (u32)(val >> 32));

    return msr;
}

static void notrace dmpatch_ibt_restore(u64 save)
{
	u64 msr;

    msr = dmpatch_rdmsr(MSR_IA32_S_CET);

	msr &= ~CET_ENDBR_EN;
	msr |= (save & CET_ENDBR_EN);

    dmpatch_wrmsr(MSR_IA32_S_CET, (u32)(msr & 0xffffffffULL), (u32)(msr >> 32));
}
#else
static u64 notrace dmpatch_ibt_save(void) { return 0; }
static void notrace dmpatch_ibt_restore(u64 save) { (void)save; }
#endif

static int notrace dmpatch_process(unsigned long a, unsigned long b, unsigned long c)
{
    int r = 0;
    int rc = 0;
    unsigned long kv_major = 0;
    unsigned long kv_minor = 0;
    unsigned long kv_subminor = 0;

    vdebug("dmpatch_process as KV %d.%d.%d ...\n", (int)a, (int)b, (int)c);

    kv_major = g_ko_param.kv_major;
    kv_minor = g_ko_param.kv_minor;
    kv_subminor = g_ko_param.kv_subminor;

    g_ko_param.kv_major = a;
    g_ko_param.kv_minor = b;
    g_ko_param.kv_subminor = c;
    
    if (dmpatch_kv_above(6, 5, 0)) /* >= kernel 6.5 */
    {
        vdebug("new interface patch dm_get_table_device...\n");
        r = dmpatch_patch_claim_ptr();
    }
    else
    {
        r = dmpatch_replace_code(1, g_ko_param.sym_get_addr, g_ko_param.sym_get_size, 2, "dm_get_table_device", g_get_patch);
        if (r && g_ko_param.kv_major >= 5)
        {
            vdebug("new2 patch dm_get_table_device...\n");
            r = dmpatch_replace_code(2, g_ko_param.sym_get_addr, g_ko_param.sym_get_size, 1, "dm_get_table_device", g_get_patch);
        }

        if (r && g_ko_param.kv_major >= 5)
        {
            vdebug("new3 patch dm_get_table_device...\n");
            r = dmpatch_replace_code(3, g_ko_param.sym_get_addr, g_ko_param.sym_get_size, 1, "dm_get_table_device", g_get_patch);
        }
    }

    if (r)
    {
        rc = -EFAULT;
        goto out;
    }
    vdebug("patch dm_get_table_device success\n");

    if (dmpatch_kv_above(6, 5, 0))
    {
        r = 0;
    }
    else
    {
        r = dmpatch_replace_code(1, g_ko_param.sym_put_addr, g_ko_param.sym_put_size, 1, "dm_put_table_device", g_put_patch);        
        if (r)
        {
            rc = -EFAULT;
            goto out;
        }
        vdebug("patch dm_put_table_device success\n");
    }

    vdebug("#####################################\n");
    vdebug("######## dm patch success ###########\n");
    vdebug("#####################################\n");

out:

    g_ko_param.kv_major = kv_major;
    g_ko_param.kv_minor = kv_minor;
    g_ko_param.kv_subminor = kv_subminor;

	return rc;
}

static int notrace dmpatch_init(void)
{
    int rc = 0;
    u64 msr = 0;
    
    if (g_ko_param.ibt == 0x8888)
    {
        msr = dmpatch_ibt_save();
    }
    
    kprintf = (printk_pf)(g_ko_param.printk_addr); 

    vdebug("dmpatch_init start pagesize=%lu kernel=%lu.%lu.%lu ...\n", 
        g_ko_param.pgsize, g_ko_param.kv_major, g_ko_param.kv_minor, g_ko_param.kv_subminor);
    
    if (g_ko_param.struct_size != sizeof(ko_param))
    {
        vdebug("Invalid struct size %d %d\n", (int)g_ko_param.struct_size, (int)sizeof(ko_param));
        return -EINVAL;
    }
    
    if (g_ko_param.sym_get_addr == 0 || g_ko_param.sym_put_addr == 0 || 
        g_ko_param.ro_addr == 0 || g_ko_param.rw_addr == 0)
    {
        return -EINVAL;
    }

    set_mem_ro = (set_memory_attr_pf)(g_ko_param.ro_addr);
    set_mem_rw = (set_memory_attr_pf)(g_ko_param.rw_addr);
    reg_kprobe = (kprobe_reg_pf)g_ko_param.reg_kprobe_addr;
    unreg_kprobe = (kprobe_unreg_pf)g_ko_param.unreg_kprobe_addr;

    rc = dmpatch_process(g_ko_param.kv_major, g_ko_param.kv_minor, g_ko_param.kv_subminor);
    if (rc)
    {
        if (g_ko_param.kv_major >= 5)
        {
            rc = dmpatch_process(6, 5, 0);
            if (rc)
            {
                rc = dmpatch_process(6, 7, 0);
            }
        }
    }

    if (g_ko_param.ibt == 0x8888)
    {
        dmpatch_ibt_restore(msr);
    }

    return rc;
}

static void notrace dmpatch_exit(void)
{
    int i = 0;
    u64 msr;

    if (g_ko_param.ibt == 0x8888)
    {
        msr = dmpatch_ibt_save();
    }

    if (g_claim_ptr)
    {
        dmpatch_restore_code(4, g_get_patch[0], g_claim_ptr);
        if (g_put_patch[0])
        {
            dmpatch_restore_code(4, g_put_patch[0], g_claim_ptr);
        }
    }
    else
    {        
        for (i = 0; i < MAX_PATCH; i++)
        {
            dmpatch_restore_code(1, g_get_patch[i], 0x80);
            dmpatch_restore_code(1, g_put_patch[i], 0x80);
        }
    }

    vdebug("dmpatch_exit success\n");

    if (g_ko_param.ibt == 0x8888)
    {
        dmpatch_ibt_restore(msr);
    }
}

module_init(dmpatch_init);
module_exit(dmpatch_exit);


MODULE_DESCRIPTION("dmpatch driver");
MODULE_AUTHOR("longpanda <admin@ventoy.net>");
MODULE_LICENSE("GPL");

