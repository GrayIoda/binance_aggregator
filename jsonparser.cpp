#include <nlohmann/json.hpp>
#include "jsonparser.h"
#include "fixedpoint.h"
#include "settings.h"

using json = nlohmann::json;

parse_binanse_json_ret parse_binanse_json(const std::string& str, const Settings& settings)
{
    // TODO: here may be place for aggressive speed optimization with simplified parser
    json j = json::parse(str);
    auto data = j.at("data");
    const std::string& s_symbol = data.at("s").get_ref<const std::string&>();
    const std::string& s_price = data.at("p").get_ref<const std::string&>();
    const std::string& s_quantity = data.at("q").get_ref<const std::string&>();
    int64_t timestamp = data.at("T").get<int64_t>();
    int64_t price = string_to_fixedpoint(s_price, settings.fixed_precision);
    int64_t quantity = string_to_fixedpoint(s_quantity, settings.fixed_precision);
    return { s_symbol, price, quantity, timestamp };
}
