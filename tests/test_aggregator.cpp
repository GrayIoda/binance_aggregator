#include <gtest/gtest.h>
#include "../aggregator.h"
#include "../settings.h"

TEST(Aggregator, TradeOnBoundary)
{
	Settings s;
	s.load(std::filesystem::path("fixed_settings_for_test.ini"));
	s.useUpperNames();
	Aggregator aggr(s);
	aggr.moveInTime(1'000'000, s);
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

TEST(Aggregator, NoTradesNoOutput)
{
	Settings s;
	s.load(std::filesystem::path("fixed_settings_for_test.ini"));
	s.useUpperNames();
	std::filesystem::remove(s.output_file);
	{
		Aggregator aggr(s);
		aggr.moveInTime(1'000'000, s);
		aggr.flushAll(s);
	}
	EXPECT_TRUE(std::filesystem::exists(s.output_file));
	EXPECT_EQ(std::filesystem::file_size(s.output_file), 0);
}

TEST(Aggregator, Aggregation)
{
	Settings s;
	s.load(std::filesystem::path("fixed_settings_for_test.ini"));
	s.useUpperNames();
	Aggregator aggr(s);
	aggr.moveInTime(1'000'000, s);
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
