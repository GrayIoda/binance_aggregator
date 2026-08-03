#include <gtest/gtest.h>
#include "../fixedpoint.h"

TEST(StringToFixedPointTest, ValidConversion) 
{
    EXPECT_EQ(string_to_fixedpoint("0", 8), 0LL);
    EXPECT_EQ(string_to_fixedpoint("0.0", 8), 0LL);
    EXPECT_EQ(string_to_fixedpoint("000012.34", 8), 1234000000LL);
    EXPECT_EQ(string_to_fixedpoint("-1.5", 8), -150000000LL);
    EXPECT_EQ(string_to_fixedpoint("100", 2), 10000LL);
}

TEST(StringToFixedPointTest, InvalidFormatThrows) 
{
    EXPECT_THROW(string_to_fixedpoint("", 8), std::invalid_argument);
    EXPECT_THROW(string_to_fixedpoint("-", 8), std::invalid_argument);
    EXPECT_THROW(string_to_fixedpoint("1..2", 8), std::invalid_argument);
    EXPECT_THROW(string_to_fixedpoint("12.34a", 8), std::invalid_argument);
}

TEST(StringToFixedPointTest, OverflowThrows) 
{
    EXPECT_THROW(string_to_fixedpoint("9223372036854775808.0", 8), std::out_of_range);
    EXPECT_THROW(string_to_fixedpoint("0.123456789", 8), std::out_of_range);
    EXPECT_THROW(string_to_fixedpoint("1.0", -1), std::out_of_range);
    EXPECT_THROW(string_to_fixedpoint("1.0", 20), std::out_of_range);
}

TEST(FixedPointToStringTest, ValidConversion)
{
    { char buf[100] = {0}; fixedpoint_to_string(0, 8, buf, 11);  EXPECT_STREQ(buf, "0.00000000"); }
    { char buf[100] = {0}; fixedpoint_to_string(1, 8, buf, 11);  EXPECT_STREQ(buf, "0.00000001"); }
    { char buf[100] = {0}; fixedpoint_to_string(-1, 8, buf, 12);  EXPECT_STREQ(buf, "-0.00000001"); }
    { char buf[100] = {0}; fixedpoint_to_string(-123456789, 8, buf, 12);  EXPECT_STREQ(buf, "-1.23456789"); }
    { char buf[100] = {0}; fixedpoint_to_string(10123456789, 8, buf, 13);  EXPECT_STREQ(buf, "101.23456789"); }
    { char buf[100] = {0}; fixedpoint_to_string(10000000000, 8, buf, 13);  EXPECT_STREQ(buf, "100.00000000"); }
    { char buf[100] = {0}; fixedpoint_to_string(1000, 8, buf, 11);  EXPECT_STREQ(buf, "0.00001000"); }
}

TEST(FixedPointToStringTest, OverflowThrows)
{
    { char buf[100] = {0}; EXPECT_THROW(fixedpoint_to_string(0, 8, buf, 10), std::out_of_range); }
    { char buf[100] = {0}; EXPECT_THROW(fixedpoint_to_string(1, 8, buf, 10), std::out_of_range); }
    { char buf[100] = {0}; EXPECT_THROW(fixedpoint_to_string(-1, 8, buf, 11), std::out_of_range); }
    { char buf[100] = {0}; EXPECT_THROW(fixedpoint_to_string(-123456789, 8, buf, 11), std::out_of_range); }
    { char buf[100] = {0}; EXPECT_THROW(fixedpoint_to_string(10123456789, 8, buf, 12), std::out_of_range); }
    { char buf[100] = {0}; EXPECT_THROW(fixedpoint_to_string(10000000000, 8, buf, 12), std::out_of_range); }
    { char buf[100] = {0}; EXPECT_THROW(fixedpoint_to_string(-1, 8, buf, 0), std::out_of_range); }
}
