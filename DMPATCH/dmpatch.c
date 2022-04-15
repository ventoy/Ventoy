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
    unsigned long padding[3];
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
#define PATCH_OP_POS    3
#define CODE_MATCH(code, i) \
    (code[i] == 0x40 && code[i + 1] == 0x80 && code[i + 2] == 0xce && code[i + 3] == 0x80)
#elif defined(CONFIG_X86_32)
#define PATCH_OP_POS    2
#define CODE_MATCH(code, i) \
    (code[i] == 0x80 && code[i + 1] == 0xca && code[i + 2] == 0x80 && code[i + 3] == 0xe8)
#else
#error "unsupported arch"
#endif

#define vdebug(fmt, args...) if(kprintf) kprintf(KERN_ERR fmt, ##args)

static int notrace dmpatch_replace_code(unsigned long addr, unsigned long size, int expect, const char *desc)
{
    int i = 0;
    int cnt = 0;
    unsigned long align;
    unsigned char *patch[MAX_PATCH];
    unsigned char *opCode = (unsigned char *)addr;

    vdebug("patch for %s 0x%lx %d\n", desc, addr, (int)size);

    for (i = 0; i < (int)size - 4; i++)
    {
        if (CODE_MATCH(opCode, i) && cnt < MAX_PATCH)
        {
            patch[cnt] = opCode + i + PATCH_OP_POS;
            cnt++;
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

static int notrace dmpatch_init(void)
{
    int r = 0;
    int rc = 0;

    kprintf = (printk_pf)(g_ko_param.printk_addr); 

    vdebug("dmpatch_init start pagesize=%lu ...\n", g_ko_param.pgsize);
    
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

    r = dmpatch_replace_code(g_ko_param.sym_get_addr, g_ko_param.sym_get_size, 2, "dm_get_table_device");
    if (r)
    {
        rc = -EINVAL;
        goto out;
    }
    vdebug("patch dm_get_table_device success\n");

    r = dmpatch_replace_code(g_ko_param.sym_put_addr, g_ko_param.sym_put_size, 1, "dm_put_table_device");
    if (r)
    {
        rc = -EINVAL;
        goto out;
    }
    vdebug("patch dm_put_table_device success\n");

    vdebug("#####################################\n");
    vdebug("######## dm patch success ###########\n");
    vdebug("#####################################\n");

out:

	return rc;
}

static void notrace dmpatch_exit(void)
{

}

module_init(dmpatch_init);
module_exit(dmpatch_exit);


MODULE_DESCRIPTION("dmpatch driver");
MODULE_AUTHOR("longpanda <admin@ventoy.net>");
MODULE_LICENSE("GPL");

