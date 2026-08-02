#pragma once

// Fixed point arithmetics to avoid heavy-weight FP libs

#include <cstdint>
#include <string_view>

// fixed point integer value = math value * 10^precision
// precision = max number of digits after '.', must be >= 1
// functions can throw std::out_of_range, std::invalid_argument
int64_t string_to_fixedpoint(std::string_view sv, int precision);
void fixedpoint_to_string(int64_t v, int precision, char* buffer, size_t buffer_size);
