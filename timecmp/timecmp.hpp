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

#ifndef TIMECMP_HPP_HEADER
#define TIMECMP_HPP_HEADER

#include <iostream>
#include <cassert>
#include <cstdint>

/*
 * Usage:
 * 
*/


/**
 * A helper template for RollingTime.
 * Helps RollingTime to be typed by the timer variable's size.
 */
namespace RollingTime_Helper {

	template<bool> struct Range;
	 
	template<int bits, typename = Range<true> >
	struct TimeParam
	{
		typedef uint64_t type;
	};
	 
	template<int bits>
	struct TimeParam<bits, Range<(bits <= 8)> >
	{
		typedef uint8_t type;
	};
	 
	template<int bits>
	struct TimeParam<bits, Range<(8 < bits && bits <= 16)> >
	{
		typedef uint16_t type;
	};
	 
	template<int bits>
	struct TimeParam<bits, Range<(16 < bits && bits <= 32)> >
	{
		typedef uint32_t type;
	};
}


/**
 * A rolling time class.
 * The timer variable can be of any size from (theoretical) 1 bit up to 64 bits.
 * The timestamp rolls over when its largest value is incremented.
 * F.ex. when 10 bit timer with value 0x3FF is incremented by 2 it gets value 0x001, and
 * value 0x001 still compares 'is greater' than 0x3FF.
 */
template <int TIMEBITS>
class RollingTime
{
public:
	typedef typename RollingTime_Helper::TimeParam<TIMEBITS>::type Type;
	static int const timebits = TIMEBITS;
	static Type const msb = static_cast<Type>(1) << (TIMEBITS - 1);
	static Type const max = ((msb - 1) << 1) + 1;
	
	
	RollingTime() : time_(0) 
	{ }
	RollingTime(Type const time) : time_(time) 
	{ }

	operator Type() const { return time_; }

	Type get(void) const { return time_; }

	bool operator > (RollingTime const &rhs) const {
		return ((rhs.time_ - time_) & max) > msb;
	}
	bool operator < (RollingTime const &rhs) const {
		return ((time_ - rhs.time_) & max) > msb;
	}
	bool operator >= (RollingTime const &rhs) const {
		return !(((time_ - rhs.time_) & msb) == msb);
	}
	bool operator <= (RollingTime const &rhs) const {
		return !(((rhs.time_ - time_) & msb) == msb);
	}


	RollingTime<TIMEBITS> &operator = (Type const &time) {
		time_ = (time & max);
		return *this;
	}
	RollingTime<TIMEBITS> &operator += (Type const &time) {
		time_ = (time_ + time) & max;
		return *this;
	}
	RollingTime<TIMEBITS> &operator += (RollingTime<TIMEBITS> const &rhs) {
		time_ = (time_ + rhs.time_) & max;
		return *this;
	}
	RollingTime<TIMEBITS> &operator -= (Type const &time) {
		time_ = (time_ - time) & max;
		return *this;
	}
	RollingTime<TIMEBITS> &operator -= (RollingTime<TIMEBITS> const &rhs) {
		time_ = (time_ - rhs.time_) & max;
		return *this;
	}

private:
	Type time_;
};


template <int TIMEBITS>
std::ostream &
operator << (std::ostream &stream, RollingTime<TIMEBITS> const &t) 
{ 
	stream << t.get();
	return stream; 
}

#endif
