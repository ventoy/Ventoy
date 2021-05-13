/*
 * Copyright (C) 2021 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Stack cookie
 *
 */

#include "wimboot.h"

/** Stack cookie */
unsigned long __stack_chk_guard;

/**
 * Construct stack cookie value
 *
 */
static __attribute__ (( noinline )) unsigned long make_cookie ( void ) {
	union {
		struct {
			uint32_t eax;
			uint32_t edx;
		} __attribute__ (( packed ));
		unsigned long tsc;
	} u;
	unsigned long cookie;

	/* We have no viable source of entropy.  Use the CPU timestamp
	 * counter, which will have at least some minimal randomness
	 * in the low bits by the time we are invoked.
	 */
	__asm__ ( "rdtsc" : "=a" ( u.eax ), "=d" ( u.edx ) );
	cookie = u.tsc;

	/* Ensure that the value contains a NUL byte, to act as a
	 * runaway string terminator.  Construct the NUL using a shift
	 * rather than a mask, to avoid losing valuable entropy in the
	 * lower-order bits.
	 */
	cookie <<= 8;

	return cookie;
}

/**
 * Initialise stack cookie
 *
 * This function must not itself use stack guard
 */
void init_cookie ( void ) {

	/* Set stack cookie value
	 *
	 * This function must not itself use stack protection, since
	 * the change in the stack guard value would trigger a false
	 * positive.
	 *
	 * There is unfortunately no way to annotate a function to
	 * exclude the use of stack protection.  We must therefore
	 * rely on correctly anticipating the compiler's decision on
	 * the use of stack protection.
	 */
	__stack_chk_guard = make_cookie();
}

/**
 * Abort on stack check failure
 *
 */
void __stack_chk_fail ( void ) {

	/* Abort program */
	die ( "Stack check failed\n" );
}
