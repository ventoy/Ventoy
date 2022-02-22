/******************************************************************************
 * vtoykmod.c  ---- ventoy kmod
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define _ull unsigned long long

#define magic_sig 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF

#define EI_NIDENT (16)

#define	EI_MAG0		0		/* e_ident[] indexes */
#define	EI_MAG1		1
#define	EI_MAG2		2
#define	EI_MAG3		3
#define	EI_CLASS	4
#define	EI_DATA		5
#define	EI_VERSION	6
#define	EI_OSABI	7
#define	EI_PAD		8

#define	ELFMAG0		0x7f		/* EI_MAG */
#define	ELFMAG1		'E'
#define	ELFMAG2		'L'
#define	ELFMAG3		'F'
#define	ELFMAG		"\177ELF"
#define	SELFMAG		4

#define	ELFCLASSNONE	0		/* EI_CLASS */
#define	ELFCLASS32	1
#define	ELFCLASS64	2
#define	ELFCLASSNUM	3

#define ELFDATANONE	0		/* e_ident[EI_DATA] */
#define ELFDATA2LSB	1
#define ELFDATA2MSB	2

#define EV_NONE		0		/* e_version, EI_VERSION */
#define EV_CURRENT	1
#define EV_NUM		2

#define ELFOSABI_NONE	0
#define ELFOSABI_LINUX	3

#define SHT_STRTAB	3

#pragma pack(1)


typedef struct
{
    unsigned char	e_ident[EI_NIDENT];	/* Magic number and other info */
    uint16_t	e_type;			/* Object file type */
    uint16_t	e_machine;		/* Architecture */
    uint32_t	e_version;		/* Object file version */
    uint32_t	e_entry;		/* Entry point virtual address */
    uint32_t	e_phoff;		/* Program header table file offset */
    uint32_t	e_shoff;		/* Section header table file offset */
    uint32_t	e_flags;		/* Processor-specific flags */
    uint16_t	e_ehsize;		/* ELF header size in bytes */
    uint16_t	e_phentsize;	/* Program header table entry size */
    uint16_t	e_phnum;		/* Program header table entry count */
    uint16_t	e_shentsize;	/* Section header table entry size */
    uint16_t	e_shnum;		/* Section header table entry count */
    uint16_t	e_shstrndx;		/* Section header string table index */
} Elf32_Ehdr;

typedef struct
{
    unsigned char	e_ident[EI_NIDENT];	/* Magic number and other info */
    uint16_t	e_type;			/* Object file type */
    uint16_t	e_machine;		/* Architecture */
    uint32_t	e_version;		/* Object file version */
    uint64_t	e_entry;		/* Entry point virtual address */
    uint64_t	e_phoff;		/* Program header table file offset */
    uint64_t	e_shoff;		/* Section header table file offset */
    uint32_t	e_flags;		/* Processor-specific flags */
    uint16_t	e_ehsize;		/* ELF header size in bytes */
    uint16_t	e_phentsize;	/* Program header table entry size */
    uint16_t	e_phnum;		/* Program header table entry count */
    uint16_t	e_shentsize;	/* Section header table entry size */
    uint16_t	e_shnum;		/* Section header table entry count */
    uint16_t	e_shstrndx;		/* Section header string table index */
} Elf64_Ehdr;

typedef struct
{
    uint32_t	sh_name;		/* Section name (string tbl index) */
    uint32_t	sh_type;		/* Section type */
    uint32_t	sh_flags;		/* Section flags */
    uint32_t	sh_addr;		/* Section virtual addr at execution */
    uint32_t	sh_offset;		/* Section file offset */
    uint32_t	sh_size;		/* Section size in bytes */
    uint32_t	sh_link;		/* Link to another section */
    uint32_t	sh_info;		/* Additional section information */
    uint32_t	sh_addralign;	/* Section alignment */
    uint32_t	sh_entsize;		/* Entry size if section holds table */
} Elf32_Shdr;

typedef struct
{
    uint32_t	sh_name;		/* Section name (string tbl index) */
    uint32_t	sh_type;		/* Section type */
    uint64_t	sh_flags;		/* Section flags */
    uint64_t	sh_addr;		/* Section virtual addr at execution */
    uint64_t	sh_offset;		/* Section file offset */
    uint64_t	sh_size;		/* Section size in bytes */
    uint32_t	sh_link;		/* Link to another section */
    uint32_t	sh_info;		/* Additional section information */
    uint64_t	sh_addralign;	/* Section alignment */
    uint64_t	sh_entsize;		/* Entry size if section holds table */
} Elf64_Shdr;

typedef struct elf32_rel {
  uint32_t	r_offset;
  uint32_t	r_info;
} Elf32_Rel;

typedef struct elf64_rel {
  uint64_t r_offset;	/* Location at which to apply the action */
  uint64_t r_info;	    /* index and type of relocation */
} Elf64_Rel;

typedef struct elf32_rela{
  uint32_t	r_offset;
  uint32_t	r_info;
  int32_t	r_addend;
} Elf32_Rela;

typedef struct elf64_rela {
  uint64_t r_offset;	/* Location at which to apply the action */
  uint64_t r_info;	    /* index and type of relocation */
  int64_t  r_addend;	/* Constant addend used to compute value */
} Elf64_Rela;


struct modversion_info {
	unsigned long crc;
	char name[64 - sizeof(unsigned long)];
};


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

static int verbose = 0;
#define debug(fmt, ...) if(verbose) printf(fmt, ##__VA_ARGS__)

static int vtoykmod_write_file(char *name, void *buf, int size)
{
    FILE *fp;

    fp = fopen(name, "wb+");
    if (!fp)
    {
        return -1;
    }
    
    fwrite(buf, 1, size, fp);
    fclose(fp);

    return 0;
}

static int vtoykmod_read_file(char *name, char **buf)
{
    int size;
    FILE *fp;
    char *databuf;

    fp = fopen(name, "rb");
    if (!fp)
    {
        debug("failed to open %s %d\n", name, errno);
        return -1;
    }
    
    fseek(fp, 0, SEEK_END);
    size = (int)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    databuf = malloc(size);
    if (!databuf)
    {
        debug("failed to open malloc %d\n", size);
        return -1;
    }
    
    fread(databuf, 1, size, fp);
    fclose(fp);

    *buf = databuf;
    return size;
}

static int vtoykmod_find_section64(char *buf, char *section, int *offset, int *len)
{
    uint16_t i;
    int cmplen;
    char *name = NULL;
    char *strtbl = NULL;
    Elf64_Ehdr *elf = NULL;
    Elf64_Shdr *sec = NULL;

    cmplen = (int)strlen(section);

    elf = (Elf64_Ehdr *)buf;
    sec = (Elf64_Shdr *)(buf + elf->e_shoff);
    strtbl = buf + sec[elf->e_shstrndx].sh_offset;

    for (i = 0; i < elf->e_shnum; i++)
    {
        name = strtbl + sec[i].sh_name;
        if (name && strncmp(name, section, cmplen) == 0)
        {
            *offset = (int)(sec[i].sh_offset);
            *len = (int)(sec[i].sh_size);
            return 0;
        }
    }

    return 1;
}

static int vtoykmod_find_section32(char *buf, char *section, int *offset, int *len)
{
    uint16_t i;
    int cmplen;
    char *name = NULL;
    char *strtbl = NULL;
    Elf32_Ehdr *elf = NULL;
    Elf32_Shdr *sec = NULL;

    cmplen = (int)strlen(section);

    elf = (Elf32_Ehdr *)buf;
    sec = (Elf32_Shdr *)(buf + elf->e_shoff);
    strtbl = buf + sec[elf->e_shstrndx].sh_offset;

    for (i = 0; i < elf->e_shnum; i++)
    {
        name = strtbl + sec[i].sh_name;
        if (name && strncmp(name, section, cmplen) == 0)
        {
            *offset = (int)(sec[i].sh_offset);
            *len = (int)(sec[i].sh_size);
            return 0;
        }
    }

    return 1;
}

static int vtoykmod_update_modcrc(char *oldmodver, int oldcnt, char *newmodver, int newcnt)
{
    int i, j;
    struct modversion_info *pold, *pnew;
    
    pold = (struct modversion_info *)oldmodver;
    pnew = (struct modversion_info *)newmodver;

    for (i = 0; i < oldcnt; i++)
    {
        for (j = 0; j < newcnt; j++)
        {
            if (strcmp(pold[i].name, pnew[j].name) == 0)
            {
                debug("CRC  0x%08lx --> 0x%08lx  %s\n", pold[i].crc, pnew[i].crc, pold[i].name);
                pold[i].crc = pnew[j].crc;
                break;
            }
        }
    }

    return 0;
}

static int vtoykmod_update_vermagic(char *oldbuf, int oldsize, char *newbuf, int newsize, int *modver)
{
    int i = 0;
    char *oldver = NULL;
    char *newver = NULL;

    *modver = 0;

    for (i = 0; i < oldsize - 9; i++)
    {
        if (strncmp(oldbuf + i, "vermagic=", 9) == 0)
        {
            oldver = oldbuf + i + 9;
            debug("Find old vermagic at %d <%s>\n", i, oldver);
            break;
        }
    }
    
    for (i = 0; i < newsize - 9; i++)
    {
        if (strncmp(newbuf + i, "vermagic=", 9) == 0)
        {
            newver = newbuf + i + 9;
            debug("Find new vermagic at %d <%s>\n", i, newver);
            break;
        }
    }

    if (oldver && newver)
    {
        memcpy(oldver, newver, strlen(newver) + 1);
        if (strstr(newver, "modversions"))
        {
            *modver = 1;
        }
    }

    return 0;
}

int vtoykmod_update(char *oldko, char *newko)
{
    int rc = 0;
    int modver = 0;
    int oldoff, oldlen;
    int newoff, newlen;
    int oldsize, newsize;
    char *newbuf, *oldbuf;

    oldsize = vtoykmod_read_file(oldko, &oldbuf);
    newsize = vtoykmod_read_file(newko, &newbuf);
    if (oldsize < 0 || newsize < 0)
    {
        return 1;
    }

    /* 1: update vermagic */
    vtoykmod_update_vermagic(oldbuf, oldsize, newbuf, newsize, &modver);

    /* 2: update modversion crc */
    if (modver)
    {
        if (oldbuf[EI_CLASS] == ELFCLASS64)
        {
            rc  = vtoykmod_find_section64(oldbuf, "__versions", &oldoff, &oldlen);
            rc += vtoykmod_find_section64(newbuf, "__versions", &newoff, &newlen);            
        }
        else
        {
            rc  = vtoykmod_find_section32(oldbuf, "__versions", &oldoff, &oldlen);
            rc += vtoykmod_find_section32(newbuf, "__versions", &newoff, &newlen);
        }

        if (rc == 0)
        {
            vtoykmod_update_modcrc(oldbuf + oldoff, oldlen / 64, newbuf + newoff, newlen / 64);
        }
    }
    else
    {
        debug("no need to proc modversions\n");
    }
    
    /* 3: update relocate address */
    if (oldbuf[EI_CLASS] == ELFCLASS64)
    {
        Elf64_Rela *oldRela, *newRela;
        
        rc  = vtoykmod_find_section64(oldbuf, ".rela.gnu.linkonce.this_module", &oldoff, &oldlen);
        rc += vtoykmod_find_section64(newbuf, ".rela.gnu.linkonce.this_module", &newoff, &newlen);
        if (rc == 0)
        {
            oldRela = (Elf64_Rela *)(oldbuf + oldoff);
            newRela = (Elf64_Rela *)(newbuf + newoff);
            
            debug("init_module rela: 0x%llx --> 0x%llx\n", (_ull)(oldRela[0].r_offset), (_ull)(newRela[0].r_offset));
            oldRela[0].r_offset = newRela[0].r_offset;
            oldRela[0].r_addend = newRela[0].r_addend;
            
            debug("cleanup_module rela: 0x%llx --> 0x%llx\n", (_ull)(oldRela[1].r_offset), (_ull)(newRela[1].r_offset));
            oldRela[1].r_offset = newRela[1].r_offset;
            oldRela[1].r_addend = newRela[1].r_addend;
        }
        else
        {
            debug("section .rela.gnu.linkonce.this_module not found\n");
        }
    }
    else
    {
        Elf32_Rel *oldRel, *newRel;
        
        rc  = vtoykmod_find_section32(oldbuf, ".rel.gnu.linkonce.this_module", &oldoff, &oldlen);
        rc += vtoykmod_find_section32(newbuf, ".rel.gnu.linkonce.this_module", &newoff, &newlen);
        if (rc == 0)
        {
            oldRel = (Elf32_Rel *)(oldbuf + oldoff);
            newRel = (Elf32_Rel *)(newbuf + newoff);

            debug("init_module rel: 0x%x --> 0x%x\n", oldRel[0].r_offset, newRel[0].r_offset);
            oldRel[0].r_offset = newRel[0].r_offset;

            debug("cleanup_module rel: 0x%x --> 0x%x\n", oldRel[0].r_offset, newRel[0].r_offset);
            oldRel[1].r_offset = newRel[1].r_offset;
        }
        else
        {
            debug("section .rel.gnu.linkonce.this_module not found\n");
        }
    }

    vtoykmod_write_file(oldko, oldbuf, oldsize);

    free(oldbuf);
    free(newbuf);

    return 0;
}

int vtoykmod_fill_param(char **argv)
{
    int i;
    int size;
    char *buf = NULL;
    ko_param *param;
    unsigned char magic[16] = { magic_sig };
    
    size = vtoykmod_read_file(argv[0], &buf);
    if (size < 0)
    {
        return 1;
    }

    for (i = 0; i < size; i++)
    {
        if (memcmp(buf + i, magic, 16) == 0)
        {
            debug("Find param magic at %d\n", i);
            param = (ko_param *)(buf + i);
            
            param->struct_size = (unsigned long)sizeof(ko_param);
            param->pgsize = strtoul(argv[1], NULL, 10);
            param->printk_addr = strtoul(argv[2], NULL, 16);
            param->ro_addr = strtoul(argv[3], NULL, 16);
            param->rw_addr = strtoul(argv[4], NULL, 16);
            param->sym_get_addr = strtoul(argv[5], NULL, 16);
            param->sym_get_size = strtoul(argv[6], NULL, 10);
            param->sym_put_addr = strtoul(argv[7], NULL, 16);
            param->sym_put_size = strtoul(argv[8], NULL, 10);
            param->reg_kprobe_addr = strtoul(argv[9], NULL, 16);
            param->unreg_kprobe_addr = strtoul(argv[10], NULL, 16);

            debug("pgsize=%lu (%s)\n", param->pgsize, argv[1]);
            debug("printk_addr=0x%lx (%s)\n", param->printk_addr, argv[2]);
            debug("ro_addr=0x%lx (%s)\n", param->ro_addr, argv[3]);
            debug("rw_addr=0x%lx (%s)\n", param->rw_addr, argv[4]);
            debug("sym_get_addr=0x%lx (%s)\n", param->sym_get_addr, argv[5]);
            debug("sym_get_size=%lu (%s)\n", param->sym_get_size, argv[6]);
            debug("sym_put_addr=0x%lx (%s)\n", param->sym_put_addr, argv[7]);
            debug("sym_put_size=%lu (%s)\n", param->sym_put_size, argv[8]);
            debug("reg_kprobe_addr=0x%lx (%s)\n", param->reg_kprobe_addr, argv[9]);
            debug("unreg_kprobe_addr=0x%lx (%s)\n", param->unreg_kprobe_addr, argv[10]);
            
            break;
        }
    }

    if (i >= size)
    {
        debug("### param magic not found \n");
    }
    
    vtoykmod_write_file(argv[0], buf, size);

    free(buf);
    return 0;
}

int vtoykmod_main(int argc, char **argv)
{
    int i;

    for (i = 0; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] == 'v')
        {
            verbose = 1;
            break;
        }
    }

    if (argv[1][0] == '-' && argv[1][1] == 'f')
    {
        return vtoykmod_fill_param(argv + 2);
    }
    else if (argv[1][0] == '-' && argv[1][1] == 'u')
    {
        return vtoykmod_update(argv[2], argv[3]);
    }

    return 0;
}

// wrapper main
#ifndef BUILD_VTOY_TOOL
int main(int argc, char **argv)
{
    return vtoykmod_main(argc, argv);
}
#endif

