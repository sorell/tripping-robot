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

#ifndef OBSCURESTR_HPP_HEADER
#define OBSCURESTR_HPP_HEADER

#include <utility>


// Sebastien Andrivet's copyright notice concerning MetaRandomGenerator class

//
//  MetaRandom.h
//  ADVobfuscator
//
// Copyright (c) 2010-2014, Sebastien Andrivet
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Get latest version on https://github.com/andrivet/ADVobfuscator

// Very simple compile-time random numbers generator.

// For a more complete and sophisticated example, see:
// http://www.researchgate.net/profile/Zalan_Szgyi/publication/259005783_Random_number_generator_for_C_template_metaprograms/file/e0b49529b48272c5a6.pdf


// 1988, Stephen Park and Keith Miller
// "Random Number Generators: Good Ones Are Hard To Find", considered as "minimal standard"
// Park-Miller 31 bit pseudo-random number generator, implemented with G. Carta's optimisation:
// with 32-bit math and without division

namespace ss_obscurestr {

template <int N>
struct MetaRandomGenerator
{
private:
    static unsigned int constexpr a = 16807;        // 7^5
    static unsigned int constexpr m = (1UL << 31) - 1;   // 2^31 - 1

    static unsigned int constexpr s = MetaRandomGenerator<N - 1>::value;
    static unsigned int constexpr lo = a * (s & 0xFFFF);                // Multiply lower 16 bits by 16807
    static unsigned int constexpr hi = a * (s >> 16);                   // Multiply higher 16 bits by 16807
    static unsigned int constexpr lo2 = lo + ((hi & 0x7FFF) << 16);     // Combine lower 15 bits of hi with lo's upper bits
    static unsigned int constexpr hi2 = hi >> 15;                       // Discard lower 15 bits of hi
    static unsigned int constexpr lo3 = lo2 + hi;
public:
    static unsigned int constexpr max = m;
    static unsigned int constexpr value = lo3 > m ? lo3 - m : lo3;
};


template <>
struct MetaRandomGenerator<-1>
{
    // Convert time string (hh:mm:ss) into a number.
    // Sami Sorell: The compilation time transformed to a randon number is an abysmally bad random number.
    // I modified the code to not hand over the seed as a first random number, so that 
    // MetaRandomGenerator<0> is the first random number.
    static unsigned int constexpr value = (__TIME__[7] - '0') + (__TIME__[6] - '0') * 10 + (__TIME__[4] - '0') * 60 +
        (__TIME__[3] - '0') * 600 + (__TIME__[1] - '0') * 3600 + (__TIME__[0] - '0') * 36000;
};


// A simple compile time byte shifter. Shift of 5 bytes makes actually a shift of 1 byte.
template <unsigned int WORD, size_t SHIFT>
struct ByteShifter
{
    static unsigned char constexpr byte = (WORD >> ((SHIFT % 4) * 8)) & 0xFF;
};


// This is ObscureStr's only function that has ram footprint. Make it global static to reduce code bloat.
static void deobscure(unsigned int key, char *const str, size_t const str_size) {
    for (char *p=str; p < str+str_size; ++p) {
        unsigned char const conversion_key = (key & 0xFF);
        *p ^= conversion_key;
        key = ((unsigned int) conversion_key << 24) | (key >> 8);
    }
}


template <typename KEY, typename IS>
struct ObscureStr;

template <typename KEY, size_t ...IS>
class ObscureStr<KEY, std::index_sequence<IS...>>
{
public:
    // A modern enough gcc allows constexpr ctor to have a function body. (5.x will do, 4.x not so much).
    constexpr __attribute__((always_inline))
    ObscureStr(char const *str) : str_{ obscure(str[IS], ByteShifter<KEY::value, IS>::byte)... } { 
    }

    char const *c_str(void) const { return str_; }

    ObscureStr<KEY, std::index_sequence<IS...>> constexpr &deobscure(void) {
        ss_obscurestr::deobscure(KEY::value, str_, sizeof...(IS)); return *this; 
    }

private:
    inline char constexpr obscure(char const ch, unsigned int const conversion_key) const { return ch ^ conversion_key; }

    char str_[sizeof...(IS)];
};

}  // namespace ss_obscurestr


// Creator helper function.
// Use this to generate obscure strings.
template <typename STR, size_t SIZE>
__attribute__((always_inline))
char const constexpr *obscureStr(STR const (&str)[SIZE])
{
    return ss_obscurestr::ObscureStr<ss_obscurestr::MetaRandomGenerator<__COUNTER__>, std::make_index_sequence<SIZE>>(str).deobscure().c_str();
}

#endif  // OBSCURESTR_HPP_HEADER
