/*
	compiler.h (09.06.13)
	Compiler-specific definitions. Note that unknown compiler is not a
	showstopper.

	Free exFAT implementation.
	Copyright (C) 2010-2018  Andrew Nayenko

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef COMPILER_H_INCLUDED
#define COMPILER_H_INCLUDED

#if __STDC_VERSION__ < 199901L
#error C99-compliant compiler is required
#endif

#if defined(__clang__)

#define PRINTF __attribute__((format(printf, 1, 2)))
#define NORETURN __attribute__((noreturn))
#define PACKED __attribute__((packed))
#if __has_extension(c_static_assert)
#define USE_C11_STATIC_ASSERT
#endif

#elif defined(__GNUC__)

#define PRINTF __attribute__((format(printf, 1, 2)))
#define NORETURN __attribute__((noreturn))
#define PACKED __attribute__((packed))
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#define USE_C11_STATIC_ASSERT
#endif

#else

#define PRINTF
#define NORETURN
#define PACKED

#endif

#ifdef USE_C11_STATIC_ASSERT
#define STATIC_ASSERT(cond) _Static_assert(cond, #cond)
#else
#define CONCAT2(a, b) a ## b
#define CONCAT1(a, b) CONCAT2(a, b)
#define STATIC_ASSERT(cond) \
	extern void CONCAT1(static_assert, __LINE__)(int x[(cond) ? 1 : -1])
#endif

#endif /* ifndef COMPILER_H_INCLUDED */
