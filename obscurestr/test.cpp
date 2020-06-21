/*
 * Copyright (C) 2016 Sami Sorell
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
#include <string>
#include <iostream>
#include "obscurestr.hpp"


void test(void)
{
    static char const *const staticText = obscureStr("ObscureStr as a static variable");
    char const *const stackText = obscureStr("ObscureStr from stack");
    std::string stdStr(obscureStr("ObscureStr to std::string"));
    
    printf("%s\n%s\n", stackText, staticText);
    printf("%s\n", stdStr.c_str());
    std::cout << obscureStr("ObscureStr to cout") << std::endl;
}


int main(void)
{
    test();
    printf("Test again to prove that the static strings are not de-obscured twice\n");
    test();
    return 0;
}
