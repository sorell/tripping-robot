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

#include <iostream>
#include <cstdint>
#include "timecmp.hpp"


template <class T> 
static int
testTimeBits(T const &rt, uint64_t const max, uint64_t const msb)
{
	std::cout << "Time of size " << std::dec << rt.timebits;
	std::cout << ", max " << std::hex << (uint64_t) rt.max << ' ' << (rt.max == max ? "ok" : "FAILED");
	std::cout << ", msb " << std::hex << (uint64_t) rt.msb << ' ' << (rt.msb == msb ? "ok" : "FAILED") << std::endl;

	return rt.max == max  &&  rt.msb == msb ? 0 : -1;
}


template <class T>
static int
testTimeLimits(T const &rt)
{
	int rv;
	int result = 0;

	struct TestParams
	{
		typename T::Type t0;
		typename T::Type t1;
		bool isAfter;
		bool isBefore;
		bool isAfterEq;
		bool isBeforeEq;
	};

	struct TestParams const params[] = {
		{0, 0, 0, 0, 1, 1},
		{1, 0, 1, 0, 1, 0},
		{0, 1, 0, 1, 0, 1},
		{(typename T::Type) (rt.max / 2),     0, 1, 0, 1, 0},
		{(typename T::Type) (rt.max / 2 + 1), 0, 0, 0, 0, 0},
		{(typename T::Type) (rt.max / 2 + 2), 0, 0, 1, 0, 1},
		{(typename T::Type) (rt.max),         0, 0, 1, 0, 1},
		{0, (typename T::Type) (rt.max / 2),     0, 1, 0, 1},
		{0, (typename T::Type) (rt.max / 2 + 1), 0, 0, 0, 0},
		{0, (typename T::Type) (rt.max / 2 + 2), 1, 0, 1, 0},
		{0, (typename T::Type) (rt.max),         1, 0, 1, 0}
	};

	for (unsigned int i = 0; i < sizeof(params) / sizeof(params[0]); ++i) {

		// Check that compiler accepts various operations
		T const t1(params[i].t1);
		T t0(t1);
		t0 += t1;
		t0 -= t1;
		t0 += t1.get();
		t0 -= t1.get();
		t0 = params[i].t0;

		std::cout << "Testing " << std::hex << (uint64_t) t0 << " vs " << (uint64_t) t1;

		result |= !(rv = (t0 > t1) == params[i].isAfter);
		std::cout << "  after " << (rv ? "ok" : "FAILED");

		result |= !(rv = (t0 < t1) == params[i].isBefore);
		std::cout << ", before " << (rv ? "ok" : " FAILED");

		result |= !(rv = (t0 >= t1) == params[i].isAfterEq);
		std::cout << ", after/eq " << (rv ? "ok" : "FAILED");

		result |= !(rv = (t0 <= t1) == params[i].isBeforeEq);
		std::cout << ", before/eq " << (rv ? "ok" : " FAILED") << std::endl;
	}

	return result;
}


int 
testCpp(void)
{
	int result = 0;

	std::cout << "Testing borderline maximum values:" << std::endl;

	result |= testTimeBits(RollingTime<7>(), 0x7F, 0x40);
	result |= testTimeBits(RollingTime<8>(), 0xFF, 0x80);
	result |= testTimeBits(RollingTime<9>(), 0x1FF, 0x100);
	result |= testTimeBits(RollingTime<15>(), 0x7FFF, 0x4000);
	result |= testTimeBits(RollingTime<16>(), 0xFFFF, 0x8000);
	result |= testTimeBits(RollingTime<31>(), 0x7FFFFFFF, 0x40000000);
	result |= testTimeBits(RollingTime<63>(), 0x7FFFFFFFFFFFFFFF, 0x4000000000000000);

	std::cout << std::endl << "Testing limits:" << std::endl;

	result |= testTimeLimits(RollingTime<7>());
	result |= testTimeLimits(RollingTime<8>());
	result |= testTimeLimits(RollingTime<9>());
	result |= testTimeLimits(RollingTime<16>());
	result |= testTimeLimits(RollingTime<32>());
	result |= testTimeLimits(RollingTime<33>());

	return result;
}
