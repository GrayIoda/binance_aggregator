#pragma once

// All JSON parsing is inside to avoid include huge nlohmann headers in other places

#include <string>
#include <cstdint>

class  Settings;

struct parse_binanse_json_ret
{
	std::string symbol;
	int64_t price;
	int64_t quantity;
	int64_t timestamp;
};

// on parse error throws exception
parse_binanse_json_ret parse_binanse_json(const std::string& str, const Settings &settings);
