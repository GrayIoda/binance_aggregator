#pragma once

// simple .ini file:
// lines in format key=value (without spaces)
// key names are the same as members of class
// ended with array of stream names after key streams= without value

#include <vector>
#include <string>
#include <string_view>
#include <filesystem>

class Settings
{
protected:

	// trade symbols
	// after loading - in lower case (to form URL)
	// later - in upper case (for search)
	std::vector<std::string> streams;

	int64_t test_time = 0;

public:

	// find symbol, return index [0, streams_count[ among streams or -1 if not found
	int getIndexFromSymbol(std::string_view sv) const;
	
	// get symbol by index
	const std::string &getSymbolByIndex(int index) const;

	// load config from file
	void load(const std::filesystem::path &path);

	// generate URL
	std::string getURLForBinance() const;

	// convert names to uppercase
	void useUpperNames();

	// steady Epoch time, ms
	int64_t getWallTime() const;

	// public shortcut for streams array size
	size_t streams_count = 0;

	// interval to flush data
	int flush_interval_ms = 5000;

	// aggregation time
	int window_ms = 1000;

	// safety time
	int safety_gap_ms = 0;

	// verbose logging
	int verbose_level = 0;

	// output file path
	std::filesystem::path output_file= "aggregator.log";

	// precision for fixed point values
	static const int fixed_precision = 8;
};
