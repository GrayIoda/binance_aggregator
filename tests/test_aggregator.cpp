#include <gtest/gtest.h>
#include "../aggregator.h"
#include "../settings.h"

class TestSettings: public Settings
{
public:
	TestSettings()
	{
		streams.push_back("BNBUSDT");
		streams.push_back("BTCUSDT");
		streams.push_back("ETHUSDT");
		streams_count = streams.size();
		test_time = 1'000'000;
	}

	// for tests only
	void setWallTime(int64_t timestamp) { test_time = timestamp; }
};

TEST(Aggregator, TradeOnBoundary)
{
	TestSettings s;
	Aggregator aggr(s);
	int64_t time_on_the_border = 1'000'000 + s.window_ms;
	aggr.addTrade("BTCUSDT", 100'000'000, 200'000'000, time_on_the_border, s);
	for (const auto& window : aggr.queue)
	{
		if (window.window_start_ms == time_on_the_border)
		{
			EXPECT_TRUE(window.streams[s.getIndexFromSymbol("BTCUSDT")].trades > 0);
			return;
		}
	}
	FAIL() << "Window is not found";
}

TEST(Aggregator, TradesWithSpan)
{
	TestSettings s;
	Aggregator aggr(s);
	aggr.addTrade("BTCUSDT", 100'000'000, 200'000'000, 5'035, s);
	aggr.addTrade("BTCUSDT", 100'000'000, 200'000'000, 7'235, s);
	EXPECT_TRUE(true); // here without exceptions
}

TEST(Aggregator, FlushTests)
{
	TestSettings s;
	std::filesystem::remove(s.output_file);
	{
		Aggregator aggr(s);
		s.setWallTime(1'000'000 + 122);
		aggr.addTrade("BTCUSDT", 100'000'000, 200'000'000, 5'035, s);
		// must add one window
		EXPECT_EQ(aggr.queue.size(), 1);
		aggr.flush(s);
		// must not flush
		EXPECT_EQ(aggr.queue.size(), 1);
		aggr.addTrade("BTCUSDT", 100'000'000, 200'000'000, 7'235, s);
		// must add 2 windows
		EXPECT_EQ(aggr.queue.size(), 3);
		s.setWallTime(1'000'000 + 5122);
		aggr.flush(s);
		// must keep last frame
		EXPECT_EQ(aggr.queue.size(), 1);
		s.setWallTime(1'000'000 + 100'000);
		aggr.flush(s);
		// like long pause on exchange - must make full flush
		EXPECT_EQ(aggr.queue.size(), 0);
	}
}

TEST(Aggregator, NoTradesNoOutput)
{
	TestSettings s;
	std::filesystem::remove(s.output_file);
	{
		Aggregator aggr(s);
		aggr.appendWindowsCoveredTimestamp(1'000'000, s);
		aggr.appendWindowsCoveredTimestamp(1'010'000, s);
		aggr.finalFlush(s);
	}
	EXPECT_TRUE(std::filesystem::exists(s.output_file));
	EXPECT_EQ(std::filesystem::file_size(s.output_file), 0);
}

TEST(Aggregator, TradesWithOutput)
{
	TestSettings s;
	std::filesystem::remove(s.output_file);
	{
		Aggregator aggr(s);
		aggr.addTrade("BTCUSDT", 100'000'000, 200'000'000, 5'035, s);
		aggr.addTrade("BTCUSDT", 100'000'000, 200'000'000, 7'035, s);
		s.setWallTime(1'000'000 + s.flush_interval_ms);
		aggr.flush(s);
	}
	EXPECT_TRUE(std::filesystem::exists(s.output_file));
	EXPECT_TRUE(std::filesystem::file_size(s.output_file) > 0);
}

TEST(Aggregator, Aggregation)
{
	TestSettings s;
	Aggregator aggr(s);
	aggr.addTrade("BTCUSDT", 100'000'000, 200'000'000, 1'000'010, s);
	aggr.addTrade("BTCUSDT", 100'000'000, 200'000'000, 1'000'030, s);
	aggr.addTrade("ETHUSDT", 100'000'000, 200'000'000, 1'000'230, s);
	aggr.addTrade("BTCUSDT", 400'000'000, 100'000'000, 1'000'430, s);
	aggr.addTrade("ETHUSDT", 100'000'000, 100'000'000, 1'000'830, s);
	const auto &window = aggr.queue.front();
	const auto& stream1 = window.streams[s.getIndexFromSymbol("BTCUSDT")];
	const auto& stream2 = window.streams[s.getIndexFromSymbol("ETHUSDT")];
	EXPECT_TRUE(stream1.trades == 3 && stream1.min_price == 100'000'000 && stream1.max_price == 400'000'000 && stream1.volume == 800'000'000);
	EXPECT_TRUE(stream2.trades == 2 && stream2.min_price == 100'000'000 && stream2.max_price == 100'000'000 && stream2.volume == 300'000'000);
}
