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
#include "timecmp.hpp"


struct TestParams
{
	unsigned long long t0;
	unsigned long long t1;
	int isAfter;
	int isBefore;
	int isAfterEq;
	int isBeforeEq;
};


int testC(void);
int testCpp(void);

int
main(void)
{
	int result;

	result = testC();
	result |= testCpp();

	std::cout << std::endl;

	if (result) {
		std::cout << "Some tests FAILED!" << std::endl;
	}
	else {
		std::cout << "All tests passed" << std::endl;	
	}


	return 0;
}
