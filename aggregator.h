#pragma once

// Class for aggregation of data
// Detached from network level

#include <limits>
#include <deque>
#include <cstdint>
#include <string_view>
#include <memory>
#include <fstream>
#include <gtest/gtest_prod.h>

class  Settings;

class Aggregator
{
	FRIEND_TEST(Aggregator, TradeOnBoundary);
public:

	// aggregated values for single stream and window
	struct Streams
	{
		int64_t trades = 0;
		int64_t min_price = std::numeric_limits<int64_t>::max();
		int64_t max_price = -1;
		int64_t volume = 0;

		// add one trade, update values
		void addTrade(int64_t price, int64_t quantity);
	};

	// aggregated values for single window
	struct Window
	{
		int64_t window_start_ms = 0;
		
		// size of array is equal to Settings::streams_count
		std::unique_ptr<Streams[]> streams;

		// dump window to specified file (must be opened)
		void dump(std::ofstream& file, const Settings &settings) const;
	};

	// windows in the time order
	// continuous sequence covers whole range [min_valid_timestamp, max_valid_timestamp[
	std::deque<Window> queue;
	
	// shortcuts
	int64_t min_valid_timestamp = 0;
	int64_t max_valid_timestamp = 0;

	// counter of invalid trades or malformed JSON-s
	int invalid_trades_counter = 0;

	// main file for dump
	std::ofstream file;

	// append specified window
	void appendWindow(int64_t window_start_ms, const Settings& settings);

public:

	// open output file inside, exception on error
	Aggregator(const Settings& settings);
	
	// add one trade
	// increment invalid_trades_counter if something wrong
	void addTrade(const std::string_view &symbol, int64_t price, int64_t quantity, int64_t timestamp, const Settings &settings);

	// flush (and ignore further) windows with max time < now
	// allocate windows for predicted time in the future
	void moveInTime(int64_t now, const Settings& settings);

	// for ^C finish
	void flushAll(const Settings& settings);

	// add error count
	void incrementErrors() { ++invalid_trades_counter; }
};
