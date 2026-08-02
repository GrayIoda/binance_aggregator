#include <gtest/gtest.h>
#include "../settings.h"

namespace fs = std::filesystem;

TEST(Settings, Load)
{
	Settings s;
	s.load(fs::path("fixed_settings_for_test.ini"));
	EXPECT_EQ(s.flush_interval_ms, 5000);
	EXPECT_EQ(s.window_ms, 1000);
	EXPECT_EQ(s.streams_count, 3);
	EXPECT_EQ(s.getSymbolByIndex(0), "bnbusdt");
	EXPECT_EQ(s.getSymbolByIndex(1), "btcusdt");
	EXPECT_EQ(s.getSymbolByIndex(2), "ethusdt");
	EXPECT_EQ(s.getIndexFromSymbol("bnbusdt"), 0);
	EXPECT_EQ(s.getIndexFromSymbol("btcusdt"), 1);
	EXPECT_EQ(s.getIndexFromSymbol("ethusdt"), 2);
	EXPECT_EQ(s.getIndexFromSymbol("ethu"), -1);
	EXPECT_EQ(s.getURLForBinance(), "wss://stream.binance.com:9443/stream?streams=bnbusdt@trade/btcusdt@trade/ethusdt@trade");
}
