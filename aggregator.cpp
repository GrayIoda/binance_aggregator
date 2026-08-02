#include "aggregator.h"
#include "aggregator.h"
#include "fixedpoint.h"
#include "settings.h"
#include <cstdint>
#include <ctime>
#include <cstdio>
#include <spdlog/spdlog.h>

// for C++20 or link with fmt there may be simper but slower ways
static const char *timeToISOString(int64_t timestamp, char* buf, size_t buf_size)
{
	std::time_t seconds = static_cast<std::time_t>(timestamp / 1000);

	std::tm tm_buf{};
#if defined(_MSC_VER)
	gmtime_s(&tm_buf, &seconds);
#else
	gmtime_r(&seconds, &tm_buf);
#endif

	std::snprintf(buf, buf_size, "%04d-%02d-%02dT%02d:%02d:%02dZ",
		tm_buf.tm_year + 1900,
		tm_buf.tm_mon + 1,
		tm_buf.tm_mday,
		tm_buf.tm_hour,
		tm_buf.tm_min,
		tm_buf.tm_sec);

	return buf;
}

void Aggregator::Streams::addTrade(int64_t price, int64_t quantity)
{
	++trades;
	// this app is for Linux, but just for case...
#if defined(__SIZEOF_INT128__)
	volume += (int64_t)(((__int128)price * quantity) / 100'000'000LL);
#else
	volume += (int64_t)(((double)price * quantity) / 100'000'000LL);
#endif
	min_price = std::min(min_price, price);
	max_price = std::max(max_price, price);
}


void Aggregator::Window::dump(std::ofstream& file, const Settings& settings) const
{
	char buffer[64];
	bool time_is_written = false;
	for (int index = 0; index < settings.streams_count; ++index)
	{
		auto& stream = streams[index];
		if (stream.trades)
		{
			// do not show timestamp if period is without trades
			if (!time_is_written)
			{
				time_is_written = true;
				file << "timestamp=" << timeToISOString(window_start_ms, buffer, sizeof(buffer)) << "\n";
			}

			file << "symbol=" << settings.getSymbolByIndex(index) 
				 << " trades=" << stream.trades ;
			fixedpoint_to_string(stream.volume, settings.fixed_precision, buffer, sizeof(buffer));
			file << " volume=" << buffer;
			fixedpoint_to_string(stream.min_price, settings.fixed_precision, buffer, sizeof(buffer));
			file << " min=" << buffer;
			fixedpoint_to_string(stream.max_price, settings.fixed_precision, buffer, sizeof(buffer));
			file << " max=" << buffer;
			file << "\n";
		}
	}
	if (settings.verbose)
		spdlog::info("Flush window: {}..{}", window_start_ms, window_start_ms + settings.window_ms);
}

Aggregator::Aggregator(const Settings& settings)
{
	file.open(settings.output_file, std::ios::app);
	if (!file.is_open())
		throw std::runtime_error("Cannot create aggregator.log");
	// TODO: "disk-full handling"
}

void Aggregator::appendWindow(int64_t window_start_ms, const Settings& settings)
{
	queue.push_back(Window());
	auto& window = queue.back();
	window.streams = std::make_unique<Streams[]>(settings.streams_count);
	window.window_start_ms = window_start_ms;
	if (settings.verbose)
		spdlog::info("Append window: {}..{}", window_start_ms, window_start_ms + settings.window_ms);
}

void Aggregator::addTrade(const std::string_view &symbol, int64_t price, int64_t quantity, int64_t timestamp, const Settings& settings)
{
	if (timestamp < min_valid_timestamp)
	{
		if (settings.verbose)
			spdlog::warn("Trade from the past: {} Limits: {} .. {}", timestamp, min_valid_timestamp, max_valid_timestamp);
		++invalid_trades_counter;
		return;
	}
	if (timestamp >= max_valid_timestamp)
	{
		if (settings.verbose)
			spdlog::warn("Trade from the future: {} Limits: {} .. {}", timestamp, min_valid_timestamp, max_valid_timestamp);
		++invalid_trades_counter;
		return;
	}
	int sym_index;
	if (quantity <= 0
     || price <= 0
	 || (sym_index = settings.getIndexFromSymbol(symbol)) < 0
	   )
	{
		if (settings.verbose)
			spdlog::warn("Invalid trade: s={} p={} q={} T={}", symbol, price, quantity, timestamp);
		++invalid_trades_counter;
		return;
	}

	int64_t index = (timestamp - min_valid_timestamp) / settings.window_ms;
	if (settings.verbose)
		spdlog::info("Add trade s={} p={} q={} T={} to window {}..{} stream {}", 
			symbol, price, quantity, timestamp, 
			queue[index].window_start_ms, queue[index].window_start_ms + settings.window_ms, sym_index);
	queue[index].streams[sym_index].addTrade(price, quantity);
}

void Aggregator::flushAll(const Settings& settings)
{
	while (!queue.empty())
	{
		queue.front().dump(file, settings);
		queue.pop_front();
	}
	file.flush();
}

void Aggregator::moveInTime(int64_t now, const Settings& settings)
{
	int64_t window_ms = settings.window_ms;
	int64_t next_now = now + settings.flush_interval_ms;
	
	min_valid_timestamp = (now / window_ms) * window_ms;
	// +2* for case if end of time range is to near to predicted next flushing time
	max_valid_timestamp = (next_now / window_ms) * window_ms + 2 * window_ms;

	// for case if current time is too close to boundary, 
	// move it a bit to past if this is allowed with safety_gap_ms > 0
	// by default no safety gap because of requirements
	while (now - min_valid_timestamp < settings.safety_gap_ms)
		min_valid_timestamp -= window_ms;

	if (settings.verbose)
		spdlog::info("Set time range: {} .. {} for specified: {} .. {}", min_valid_timestamp, max_valid_timestamp, now, next_now);

	if (settings.verbose && now - min_valid_timestamp < 100)
		spdlog::warn("Window border is very close to current time");

	// dump and remove too old
	while(!queue.empty())
	{
		auto& window = queue.front();
		if (window.window_start_ms >= min_valid_timestamp)
			break;
		window.dump(file, settings);
		queue.pop_front();
	}

	// place new
	for (int64_t window_start_ms = queue.empty() ? min_valid_timestamp : queue.back().window_start_ms + window_ms; 
		 window_start_ms < max_valid_timestamp ; window_start_ms += window_ms)
	{
		appendWindow(window_start_ms, settings);
	}

	// invalid trades
	if (invalid_trades_counter)
	{
		char buffer[64];
		spdlog::warn("Invalid trades: {} before {}", invalid_trades_counter, timeToISOString(Settings::getEpochTime(), buffer, sizeof(buffer)));
		invalid_trades_counter = 0;
	}
}

