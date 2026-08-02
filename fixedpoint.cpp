#include "fixedpoint.h"
#include <stdexcept>
#include <charconv>
#include <cstring>

// TODO "micro-optimization" is still possible
// but current version is OK

// int64 "full" digits
static const int INT64_MAX_DIGITS = 18;

int64_t string_to_fixedpoint(std::string_view sv, int precision)
{
	if (precision <= 0 || precision > INT64_MAX_DIGITS)
		throw std::out_of_range("Invalid precision");
	bool negative = false;
	if (!sv.empty())
	{
		char first = sv[0];
		if (first == '-')
		{
			negative = true;
			sv.remove_prefix(1);
		}
		else if (first == '+')
			sv.remove_prefix(1);
	}
	if (sv.empty()) // empty or just "-"/"+"
		throw std::invalid_argument("Invalid fixed point number: empty string");

	const char* from = sv.data();
	const char* to = sv.data() + sv.size();
	// pass 0s
	while (from < to && *from == '0')
		++from;

	int64_t value = 0;
	int digits_total = 0;
	int digits_after_dot = 0;
	bool dot_found = false;
	for (; from < to; ++from)
	{
		char c = *from;
		if (c == '.')
		{
			if (dot_found)
				throw std::invalid_argument("Invalid fixed point number: double point");
			dot_found = true;
		}
		else if (c >= '0' && c <= '9')
		{
			if (++digits_total > INT64_MAX_DIGITS)
				throw std::out_of_range("Fixed point number total digits overflow");
			if (dot_found && ++digits_after_dot > precision)
				throw std::out_of_range("Fixed point number fraction digits overflow");
			value = 10 * value + (c - '0');
		}
		else
			throw std::invalid_argument("Invalid character in fixed point number");
	}
	for (; digits_after_dot < precision; ++digits_after_dot)
		value *= 10;

	return negative ? -value : value;
}

void fixedpoint_to_string(int64_t v, int precision, char* buffer, size_t buffer_size)
{
	if (precision < 0 || precision > INT64_MAX_DIGITS)
		throw std::out_of_range("Invalid precision");
	auto buffer_end = buffer + buffer_size;
	auto [ptr, ec] = std::to_chars(buffer, buffer_end, v);
	if (ec != std::errc{})
		throw std::out_of_range("Buffer overflow");
	// align and insert '.'
	size_t num_of_digits = ptr - buffer;
	size_t rest_of_buf = buffer_end - ptr;
	if (buffer[0] == '-')
		--num_of_digits;
	char* from = nullptr; // insert from here
	if (num_of_digits > (size_t)precision)
	{
		if (rest_of_buf < 2) //'.' and '\0'
			throw std::out_of_range("Buffer overflow");
		from = ptr - precision;
		std::memmove(from + 1, from, precision);
	}
	else
	{
		auto zeroes_to_insert = precision - num_of_digits;
		if (rest_of_buf < 3 + zeroes_to_insert) //'0.' and '\0'
			throw std::out_of_range("Buffer overflow");
		from = ptr - num_of_digits;
		std::memmove(from + zeroes_to_insert + 2, from, num_of_digits);
		*from++ = '0';
		std::memset(from + 1, '0', zeroes_to_insert);
	}
	*from++ = '.';
	from[precision] = '\0';
}

