#include <gtest/gtest.h>
#include "../jsonparser.h"
#include "../settings.h"

TEST(JSONParser, ParseValidBinanseJSON)
{
	char valid_json[] = "{\"stream\":\"btcusdt@trade\",\"data\":{\"e\":\"trade\",\"E\":1785664681773,\"s\":\"BTCUSDT\",\"t\":6551467330,\"p\":\"63273.94000000\",\"q\":\"0.03393000\",\"T\":1785664681773,\"m\":false,\"M\":true}}";
	Settings settings;
	parse_binanse_json_ret ret = parse_binanse_json(valid_json, settings);
	EXPECT_EQ(ret.symbol, "BTCUSDT");
	EXPECT_EQ(ret.price, 6327394000000);
	EXPECT_EQ(ret.quantity, 3393000);
	EXPECT_EQ(ret.timestamp, 1785664681773);
}

TEST(JSONParser, ParseInvalidBinanseJSON)
{
	char invalid_json1[] = "{\"stream\":\"btcusdt@trade\",\"dlata\":{\"e\":\"trade\",\"E\":1785664681773,\"s\":\"BTCUSDT\",\"t\":6551467330,\"p\":\"63273.94000000\",\"q\":\"0.03393000\",\"T\":1785664681773,\"m\":false,\"M\":true}}";
	char invalid_json2[] = "{\"stream\":\"btcusdt@trade\",\"data\":{\"e\":\"trade\",\"E\":1785664681773,\"s\":\"BTCUSDT\",\"t\":6551467330,\"p\":\"63273.94000000\",\"T\":1785664681773,\"m\":false,\"M\":true}}";
	char invalid_json3[] = "{\"stream\":\"btcusdt@trade\",\"data\":{\"e\":\"trade\",\"E\":1785664681773,\"s\":\"BTCUSDT\",\"t\":6551467330,\"p\":\"63273.94000000\",\"T\":\"1785664681773\",\"m\":false,\"M\":true}}";
	Settings settings;
	EXPECT_THROW(parse_binanse_json(invalid_json1, settings), std::exception);
	EXPECT_THROW(parse_binanse_json(invalid_json2, settings), std::exception);
	EXPECT_THROW(parse_binanse_json(invalid_json3, settings), std::exception);
}
