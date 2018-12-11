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
    static char const *const staticText = obscureStr("Static variable");
    char const *const stackText = obscureStr("Variable from stack");
    std::string stdStr(obscureStr("std::string also works, why wouldn't it?"));
    
    printf("%s, %s\n", stackText, staticText);
    printf("%s\n", stdStr.c_str());
    std::cout << obscureStr("Text to cout") << std::endl;
    printf("\n");
}


int main(void)
{
    test();
    test();  // Test again to see that the strings are not de-obscured twice
    return 0;
}
