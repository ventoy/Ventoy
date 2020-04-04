// bpf.c: bpf packet filter for linux/freebsd

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "dat.h"
#include "fns.h"

struct bpf_insn {
	ushort code;
	uchar jt;
	uchar jf;
	u_int32_t k;
};

struct bpf_program {
	uint bf_len;
	struct bpf_insn *bf_insns;
};

/* instruction classes */
#define		BPF_CLASS(code) ((code) & 0x07)
#define		BPF_LD		0x00
#define		BPF_LDX		0x01
#define		BPF_ST		0x02
#define		BPF_STX		0x03
#define		BPF_ALU		0x04
#define		BPF_JMP		0x05
#define		BPF_RET		0x06
#define		BPF_MISC	0x07

/* ld/ldx fields */
#define		BPF_SIZE(code)	((code) & 0x18)
#define		BPF_W		0x00
#define		BPF_H		0x08
#define		BPF_B		0x10
#define		BPF_MODE(code)	((code) & 0xe0)
#define		BPF_IMM 	0x00
#define		BPF_ABS		0x20
#define		BPF_IND		0x40
#define		BPF_MEM		0x60
#define		BPF_LEN		0x80
#define		BPF_MSH		0xa0

/* alu/jmp fields */
#define		BPF_OP(code)	((code) & 0xf0)
#define		BPF_ADD		0x00
#define		BPF_SUB		0x10
#define		BPF_MUL		0x20
#define		BPF_DIV		0x30
#define		BPF_OR		0x40
#define		BPF_AND		0x50
#define		BPF_LSH		0x60
#define		BPF_RSH		0x70
#define		BPF_NEG		0x80
#define		BPF_JA		0x00
#define		BPF_JEQ		0x10
#define		BPF_JGT		0x20
#define		BPF_JGE		0x30
#define		BPF_JSET	0x40
#define		BPF_SRC(code)	((code) & 0x08)
#define		BPF_K		0x00
#define		BPF_X		0x08

/* ret - BPF_K and BPF_X also apply */
#define		BPF_RVAL(code)	((code) & 0x18)
#define		BPF_A		0x10

/* misc */
#define		BPF_MISCOP(code) ((code) & 0xf8)
#define		BPF_TAX		0x00
#define		BPF_TXA		0x80

/* macros for insn array initializers */
#define BPF_STMT(code, k) { (ushort)(code), 0, 0, k }
#define BPF_JUMP(code, k, jt, jf) { (ushort)(code), jt, jf, k }

void *
create_bpf_program(int shelf, int slot)
{
	struct bpf_program *bpf_program;
	struct bpf_insn insns[] = {
		/* CHECKTYPE: Load the type into register */
		BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 12),
		/* Does it match AoE Type (0x88a2)? No, goto INVALID */
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x88a2, 0, 10),
		/* Load the flags into register */
		BPF_STMT(BPF_LD+BPF_B+BPF_ABS, 14),
		/* Check to see if the Resp flag is set */
		BPF_STMT(BPF_ALU+BPF_AND+BPF_K, Resp),
		/* Yes, goto INVALID */
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0, 0, 7),
		/* CHECKSHELF: Load the shelf number into register */
		BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 16),
		/* Does it match shelf number? Yes, goto CHECKSLOT */
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, shelf, 1, 0),
		/* Does it match broadcast? No, goto INVALID */
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0xffff, 0, 4),
		/* CHECKSLOT: Load the slot number into register */
		BPF_STMT(BPF_LD+BPF_B+BPF_ABS, 18),
		/* Does it match shelf number? Yes, goto VALID */
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, slot, 1, 0),
		/* Does it match broadcast? No, goto INVALID */
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0xff, 0, 1),
		/* VALID: return -1 (allow the packet to be read) */
		BPF_STMT(BPF_RET+BPF_K, -1),
		/* INVALID: return 0 (ignore the packet) */
		BPF_STMT(BPF_RET+BPF_K, 0),
	};
	if ((bpf_program = malloc(sizeof(struct bpf_program))) == NULL
	    || (bpf_program->bf_insns = malloc(sizeof(insns))) == NULL) {
		perror("malloc");
		exit(1);
	}
	bpf_program->bf_len = sizeof(insns)/sizeof(struct bpf_insn);
	memcpy(bpf_program->bf_insns, insns, sizeof(insns));
	return (void *)bpf_program;
}

void
free_bpf_program(void *bpf_program)
{
	free(((struct bpf_program *) bpf_program)->bf_insns);
	free(bpf_program);
}
