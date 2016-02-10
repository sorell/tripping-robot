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


template <int TIMEBITS>
class RollingTime
{
public:
	typedef typename RollingTime_Helper::TimeParam<TIMEBITS>::type Type;
	int const timebits = TIMEBITS;
	Type const msb = static_cast<Type>(1) << (TIMEBITS - 1);
	Type const max = ((msb - 1) << 1) + 1;
	
	
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
