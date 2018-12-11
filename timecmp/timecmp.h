/*
 * Copyright (C) 2015 Sami Sorell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef TIMECMP_H_HEADER
#define TIMECMP_H_HEADER

#include <stdint.h>

/*
 * Usage:
 * 
*/


/**
 * For a given mask, return a word that has only the mask's msb as 1.
 * F.ex. if mask = 0x7FFF, return 0x4000.
 */
__attribute__((always_inline)) static inline uint32_t
maskMsb(uint32_t const mask)
{
	if (((mask + 1) & mask) != 0) {
		abort();  // Or your method of choice
	}
	return ((mask >> 1) + 1);
}


/**
 * For a given mask, return a word that has only the mask's msb as 1.
 * F.ex. if mask = 0x7FFF, return 0x4000.
 */
__attribute__((always_inline)) static inline uint64_t
maskMsb64(uint64_t const mask)
{
	if (((mask + 1) & mask) != 0) {
		abort();  // Or your method of choice
	}
	return ((mask >> 1) + 1);
}


/**
 * Timestamp comparison functions for 32 and 64 bit words.
 * These can be optimized further by #defining timeMax and removing its variable from the function call.
 */
__attribute__((always_inline)) static inline int
timeIsAfter(uint32_t const a, uint32_t const b, uint32_t const timeMax)
{
	return ((b - a) & timeMax) > maskMsb(timeMax);
}


__attribute__((always_inline)) static inline int
timeIsBefore(uint32_t const a, uint32_t const b, uint32_t const timeMax)
{
	return timeIsAfter(b, a, timeMax);
}


__attribute__((always_inline)) static inline int
timeIsAfterEq(uint32_t const a, uint32_t const b, uint32_t const timeMax)
{
	return !(((a - b) & maskMsb(timeMax)) == maskMsb(timeMax));
}


__attribute__((always_inline)) static inline int
timeIsBeforeEq(uint32_t const a, uint32_t const b, uint32_t const timeMax)
{
	return timeIsAfterEq(b, a, timeMax);
}


__attribute__((always_inline)) static inline int
timeIsAfter64(uint64_t const a, uint64_t const b, uint64_t const timeMax)
{
	return ((b - a) & timeMax) > maskMsb64(timeMax);
}


__attribute__((always_inline)) static inline int
timeIsBefore64(uint64_t const a, uint64_t const b, uint64_t const timeMax)
{
	return timeIsAfter64(b, a, timeMax);
}


__attribute__((always_inline)) static inline int
timeIsAfterEq64(uint64_t const a, uint64_t const b, uint64_t const timeMax)
{
	return !(((a - b) & maskMsb64(timeMax)) == maskMsb64(timeMax));
}


__attribute__((always_inline)) static inline int
timeIsBeforeEq64(uint64_t const a, uint64_t const b, uint64_t const timeMax)
{
	return timeIsAfterEq64(b, a, timeMax);
}


#endif
