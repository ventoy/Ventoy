#ifndef _BOOTAPP_H
#define _BOOTAPP_H

/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/**
 * @file
 *
 * Boot application data structures
 *
 */

#include <stdint.h>

/** A segment:offset address */
struct segoff {
	/** Offset */
	uint16_t offset;
	/** Segment */
	uint16_t segment;
} __attribute__ (( packed ));

/** A GUID */
struct guid {
	/** 8 hex digits, big-endian */
	uint32_t a;
	/** 2 hex digits, big-endian */
	uint16_t b;
	/** 2 hex digits, big-endian */
	uint16_t c;
	/** 2 hex digits, big-endian */
	uint16_t d;
	/** 12 hex digits, big-endian */
	uint8_t e[6];
} __attribute__ (( packed ));

/** Real-mode callback parameters */
struct bootapp_callback_params {
	/** Vector */
	union {
		/** Interrupt number */
		uint32_t interrupt;
		/** Segment:offset address of real-mode function */
		struct segoff function;
	} vector;
	/** %eax value */
	union {
		struct {
			uint8_t al;
			uint8_t ah;
		} __attribute__ (( packed ));
		uint16_t ax;
		uint32_t eax;
	};
	/** %ebx value */
	union {
		struct {
			uint8_t bl;
			uint8_t bh;
		} __attribute__ (( packed ));
		uint16_t bx;
		uint32_t ebx;
	};
	/** %ecx value */
	union {
		struct {
			uint8_t cl;
			uint8_t ch;
		} __attribute__ (( packed ));
		uint16_t cx;
		uint32_t ecx;
	};
	/** %edx value */
	union {
		struct {
			uint8_t dl;
			uint8_t dh;
		} __attribute__ (( packed ));
		uint16_t dx;
		uint32_t edx;
	};
	/** Placeholder (for %esp?) */
	uint32_t unused_esp;
	/** Placeholder (for %ebp?) */
	uint32_t unused_ebp;
	/** %esi value */
	union {
		uint16_t si;
		uint32_t esi;
	};
	/** %edi value */
	union {
		uint16_t di;
		uint32_t edi;
	};
	/** Placeholder (for %cs?) */
	uint32_t unused_cs;
	/** %ds value */
	uint32_t ds;
	/** Placeholder (for %ss?) */
	uint32_t unused_ss;
	/** %es value */
	uint32_t es;
	/** %fs value */
	uint32_t fs;
	/** %gs value */
	uint32_t gs;
	/** eflags value */
	uint32_t eflags;
} __attribute__ (( packed ));

/** eflags bits */
enum eflags {
	CF = ( 1 << 0 ),
	PF = ( 1 << 2 ),
	AF = ( 1 << 4 ),
	ZF = ( 1 << 6 ),
	SF = ( 1 << 7 ),
	OF = ( 1 << 11 ),
};

/** Real-mode callback function table */
struct bootapp_callback_functions {
	/**
	 * Call an arbitrary real-mode interrupt
	 *
	 * @v params		Parameters
	 */
	void ( * call_interrupt ) ( struct bootapp_callback_params *params );
	/**
	 * Call an arbitrary real-mode function
	 *
	 * @v params		Parameters
	 */
	void ( * call_real ) ( struct bootapp_callback_params *params );
} __attribute__ (( packed ));

/** Real-mode callbacks */
struct bootapp_callback {
	/** Real-mode callback function table */
	struct bootapp_callback_functions *fns;
	/** Drive number for INT13 calls */
	uint32_t drive;
} __attribute__ (( packed ));

/** Boot application descriptor */
struct bootapp_descriptor {
	/** Signature */
	char signature[8];
	/** Version */
	uint32_t version;
	/** Total length */
	uint32_t len;
	/** COFF machine type */
	uint32_t arch;
	/** Reserved */
	uint32_t reserved_0x14;
	/** Loaded PE image base address */
	void *pe_base;
	/** Reserved */
	uint32_t reserved_0x1c;
	/** Length of loaded PE image */
	uint32_t pe_len;
	/** Offset to memory descriptor */
	uint32_t memory;
	/** Offset to boot application entry descriptor */
	uint32_t entry;
	/** Offset to ??? */
	uint32_t xxx;
	/** Offset to callback descriptor */
	uint32_t callback;
	/** Offset to pointless descriptor */
	uint32_t pointless;
	/** Reserved */
	uint32_t reserved_0x38;
} __attribute__ (( packed ));

/** "BOOT APP" magic signature */
#define BOOTAPP_SIGNATURE "BOOT APP"

/** Boot application descriptor version */
#define BOOTAPP_VERSION 2

/** i386 architecture */
#define BOOTAPP_ARCH_I386 0x014c

/** Memory region descriptor */
struct bootapp_memory_region {
	/** Reserved (for struct list_head?) */
	uint8_t reserved[8];
	/** Start page address */
	uint64_t start_page;
	/** Reserved */
	uint8_t reserved_0x10[8];
	/** Number of pages */
	uint64_t num_pages;
	/** Reserved */
	uint8_t reserved_0x20[4];
	/** Flags */
	uint32_t flags;
} __attribute__ (( packed ));

/** Memory descriptor */
struct bootapp_memory_descriptor {
	/** Version */
	uint32_t version;
	/** Length of descriptor (excluding region descriptors) */
	uint32_t len;
	/** Number of regions */
	uint32_t num_regions;
	/** Length of each region descriptor */
	uint32_t region_len;
	/** Length of reserved area at start of each region descriptor */
	uint32_t reserved_len;
} __attribute__ (( packed ));

/** Boot application memory descriptor version */
#define BOOTAPP_MEMORY_VERSION 1

/** Boot application entry descriptor */
struct bootapp_entry_descriptor {
	/** Signature */
	char signature[8];
	/** Flags */
	uint32_t flags;
	/** GUID */
	struct guid guid;
	/** Reserved */
	uint8_t reserved[16];
} __attribute__ (( packed ));

/** ??? */
struct bootapp_entry_wtf1_descriptor {
	/** Flags */
	uint32_t flags;
	/** Length of descriptor */
	uint32_t len;
	/** Total length of following descriptors within BTAPENT */
	uint32_t extra_len;
	/** Reserved */
	uint8_t reserved[12];
} __attribute__ (( packed ));

/** ??? */
struct bootapp_entry_wtf2_descriptor {
	/** GUID */
	struct guid guid;
} __attribute__ (( packed ));

/** ??? */
struct bootapp_entry_wtf3_descriptor {
	/** Flags */
	uint32_t flags;
	/** Reserved */
	uint32_t reserved_0x04;
	/** Length of descriptor */
	uint32_t len;
	/** Reserved */
	uint32_t reserved_0x0c;
	/** Boot partition offset (in bytes) */
	uint32_t boot_partition_offset;
	/** Reserved */
	uint8_t reserved_0x14[16];
	/** MBR signature present? */
	uint32_t xxx;
	/** MBR signature */
	uint32_t mbr_signature;
	/** Reserved */
	uint8_t reserved_0x2c[26];
} __attribute__ (( packed ));

/** "BTAPENT" magic signature */
#define BOOTAPP_ENTRY_SIGNATURE "BTAPENT\0"

/** Boot application entry flags
 *
 * pxeboot, etftboot, and fatboot all use a value of 0x21; I have no
 * idea what it means.
 */
#define BOOTAPP_ENTRY_FLAGS 0x21

/** Boot application callback descriptor */
struct bootapp_callback_descriptor {
	/** Real-mode callbacks */
	struct bootapp_callback *callback;
	/** Reserved */
	uint32_t reserved;
} __attribute__ (( packed ));

/** Boot application pointless descriptor */
struct bootapp_pointless_descriptor {
	/** Version */
	uint32_t version;
	/** Reserved */
	uint8_t reserved[24];
} __attribute__ (( packed ));

/** Boot application pointless descriptor version */
#define BOOTAPP_POINTLESS_VERSION 1

#endif /* _BOOTAPP_H */
