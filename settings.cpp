#include <algorithm>
#include <fstream>
#include <charconv>
#include <string>
#include "settings.h"

inline constexpr size_t npos = std::string::npos;

int Settings::getIndexFromSymbol(std::string_view sv) const
{
	//binary search is not good for short arrays
	if (streams_count <= 3)
	{
		int index = 0;
		for (const auto& stream : streams)
		{
			if (stream == sv)
				return index;
			++index;
		}
		return -1;
	}
	else
	{
		auto it = std::lower_bound(streams.begin(), streams.end(), sv);
		if (it != streams.end() && *it == sv)
			return (int)std::distance(streams.begin(), it);
	}
	return -1;
}

const std::string& Settings::getSymbolByIndex(int index) const
{
	return streams.at(index);
}

void Settings::load(const std::filesystem::path& path)
{
	// TODO: "exhaustive configuration validation"
	std::ifstream file(path);
	if (!file.is_open())
		throw std::runtime_error("settings.ini is not found");
	bool streams_part_started = false;
	for(;;)
	{
		std::string line;
		if (!std::getline(file, line)) // simplify
			break;
		if (streams_part_started)
		{
			streams.push_back(line);
		}
		else
		{
			auto eq_pos = line.find('=');
			if (eq_pos != npos)
			{
				std::string_view name(line), value(line);
				name.remove_suffix(line.size() - eq_pos);
				value.remove_prefix(eq_pos + 1);
				if (name == "window_ms")
					std::from_chars(value.data(), value.data() + value.size(), window_ms);
				else if (name == "flush_interval_ms")
					std::from_chars(value.data(), value.data() + value.size(), flush_interval_ms);
				else if (name == "safety_gap_ms")
					std::from_chars(value.data(), value.data() + value.size(), safety_gap_ms);
				else if (name == "verbose")
					verbose = value == "true" || value == "TRUE";
				else if (name == "output_file")
					output_file = value;
				else if (name == "streams")
					streams_part_started = true;
				//else ignore
			}
		}
	}

	// for safety
	if (window_ms <= 10 || window_ms >= 10000000)
		window_ms = 500;
	if (flush_interval_ms <= 10 || flush_interval_ms >= 10000000)
		flush_interval_ms = 1000;

	// for search
	std::sort(streams.begin(), streams.end());
	streams_count = streams.size();
}

std::string Settings::getURLForBinance() const
{
	std::string ret = "wss://stream.binance.com:9443/stream?streams=";
	int index = 0;
	for (const auto& stream : streams)
	{
		if (index)
			ret += '/';
		ret += stream;
		ret += "@trade";
		++index;
	}
	return ret;
}

void Settings::useUpperNames()
{
	for (auto& stream : streams)
	{
		// do not like transform :)
		for (auto& c : stream)
		{
			if (c >= 'a' && c <= 'z')
				c -= 'z' - 'Z';
		}
	}
}

int64_t Settings::getEpochTime()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
