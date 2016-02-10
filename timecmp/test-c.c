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

#include <stdio.h>
#include <stdlib.h>
#include "timecmp.h"


struct TestParams
{
	uint32_t t0;
	uint32_t t1;
	int isAfter;
	int isBefore;
	int isAfterEq;
	int isBeforeEq;
};

struct TestParams64
{
	uint64_t t0;
	uint64_t t1;
	int isAfter;
	int isBefore;
	int isAfterEq;
	int isBeforeEq;
};


static int
test32(void)
{
	int rv;
	int result = 0;
	uint32_t const masks[] = {0xFF, 0xFFFF, 0x7FFFFFFF};

	for (unsigned int m = 0; m < sizeof(masks) / sizeof(masks[0]); ++m) {

		fprintf(stderr, "Mask is %X, msb %X\n", masks[m], maskMsb(masks[m]));

		struct TestParams const params[] = {
			{0, 0, 0, 0, 1, 1},
			{1, 0, 1, 0, 1, 0},
			{0, 1, 0, 1, 0, 1},
			{masks[m] / 2,     0, 1, 0, 1, 0},
			{masks[m] / 2 + 1, 0, 0, 0, 0, 0},
			{masks[m] / 2 + 2, 0, 0, 1, 0, 1},
			{masks[m],         0, 0, 1, 0, 1},
			{0, masks[m] / 2,     0, 1, 0, 1},
			{0, masks[m] / 2 + 1, 0, 0, 0, 0},
			{0, masks[m] / 2 + 2, 1, 0, 1, 0},
			{0, masks[m],         1, 0, 1, 0}
		};

		for (unsigned int i = 0; i < sizeof(params) / sizeof(params[0]); ++i) {
			fprintf(stderr, "Testing %16X vs %-16X", params[i].t0, params[i].t1);

			result |= !(rv = timeIsAfter(params[i].t0, params[i].t1, masks[m]) == params[i].isAfter);
			fprintf(stderr, "  after %s", rv ? "ok" : "FAILED");

			result |= !(rv = timeIsBefore(params[i].t0, params[i].t1, masks[m]) == params[i].isBefore);
			fprintf(stderr, ", before %s", rv ? "ok" : "FAILED");

			result |= !(rv = timeIsAfterEq(params[i].t0, params[i].t1, masks[m]) == params[i].isAfterEq);
			fprintf(stderr, ", after/eq %s", rv ? "ok" : "FAILED");

			result |= !(rv = timeIsBeforeEq(params[i].t0, params[i].t1, masks[m]) == params[i].isBeforeEq);
			fprintf(stderr, ", before/eq %s\n", rv ? "ok" : "FAILED");

		}
	}

	return result;
}


static int
test64(void)
{
	int rv;
	int result = 0;
	uint64_t const masks[] = {0x1FFFFFFFFF};

	for (unsigned int m = 0; m < sizeof(masks) / sizeof(masks[0]); ++m) {

		fprintf(stderr, "Mask is %lX, msb %lX\n", masks[m], maskMsb64(masks[m]));

		struct TestParams64 const params[] = {
			{0, 0, 0, 0, 1, 1},
			{1, 0, 1, 0, 1, 0},
			{0, 1, 0, 1, 0, 1},
			{masks[m] / 2,     0, 1, 0, 1, 0},
			{masks[m] / 2 + 1, 0, 0, 0, 0, 0},
			{masks[m] / 2 + 2, 0, 0, 1, 0, 1},
			{masks[m],         0, 0, 1, 0, 1},
			{0, masks[m] / 2,     0, 1, 0, 1},
			{0, masks[m] / 2 + 1, 0, 0, 0, 0},
			{0, masks[m] / 2 + 2, 1, 0, 1, 0},
			{0, masks[m],         1, 0, 1, 0}
		};

		for (unsigned int i = 0; i < sizeof(params) / sizeof(params[0]); ++i) {
			fprintf(stderr, "Testing %16lX vs %-16lX", params[i].t0, params[i].t1);

			result |= !(rv = timeIsAfter64(params[i].t0, params[i].t1, masks[m]) == params[i].isAfter);
			fprintf(stderr, "  after %s", rv ? "ok" : "FAILED");

			result |= !(rv = timeIsBefore64(params[i].t0, params[i].t1, masks[m]) == params[i].isBefore);
			fprintf(stderr, ", before %s", rv ? "ok" : "FAILED");

			result |= !(rv = timeIsAfterEq64(params[i].t0, params[i].t1, masks[m]) == params[i].isAfterEq);
			fprintf(stderr, ", after/eq %s", rv ? "ok" : "FAILED");

			result |= !(rv = timeIsBeforeEq64(params[i].t0, params[i].t1, masks[m]) == params[i].isBeforeEq);
			fprintf(stderr, ", before/eq %s\n", rv ? "ok" : "FAILED");

		}
	}

	return result;
}


int 
testC(void)
{
	int result = 0;

	result = test32();
	result |= test64();

	return result;
}
