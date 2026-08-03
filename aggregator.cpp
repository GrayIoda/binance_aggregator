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
	if (settings.verbose_level >= 1)
		spdlog::info("Flush window: {}..{}", window_start_ms, window_start_ms + settings.window_ms);
}

Aggregator::Aggregator(const Settings& settings)
{
	file.open(settings.output_file, std::ios::app);
	if (!file.is_open())
		throw std::runtime_error("Cannot create aggregator.log");
	last_wall_time_of_flush = settings.getWallTime();
	// TODO: "disk-full handling"
}

void Aggregator::appendWindowsCoveredTimestamp(int64_t timestamp, const Settings& settings)
{
	if (settings.verbose_level >= 1)
		spdlog::info("Adjust queue to cover time: {}", timestamp);

	int64_t window_ms = settings.window_ms;
	// frames to create
	int64_t frames_start = (timestamp / window_ms) * window_ms;
	int64_t frames_stop = frames_start + window_ms;

	// for case if current time is too close to boundary, 
	// move it a bit to past if this is allowed with safety_gap_ms > 0
	if (timestamp - frames_start < settings.safety_gap_ms)
		frames_start -= window_ms;

	// adjust according current queue content
	if (queue.empty())
	{
		min_valid_timestamp = frames_start;
		max_valid_timestamp = frames_stop;
	}
	else
	{
		frames_start = max_valid_timestamp;
		if (frames_stop > max_valid_timestamp)
			max_valid_timestamp = frames_stop;
	}

	if (settings.verbose_level >= 1 && timestamp - min_valid_timestamp < 100)
		spdlog::warn("Windows start border is very close to current time");

	// place new
	for (int64_t window_start_ms = frames_start; window_start_ms < frames_stop; )
	{
		queue.push_back(Window());
		auto& window = queue.back();
		window.streams = std::make_unique<Streams[]>(settings.streams_count);
		window.window_start_ms = window_start_ms;
		auto window_stop_ms = window_start_ms + window_ms;
		if (settings.verbose_level >= 1)
			spdlog::info("Append window: {}..{}", window_start_ms, window_stop_ms);
		window_start_ms = window_stop_ms;
	}
}

void Aggregator::addTrade(const std::string_view &symbol, int64_t price, int64_t quantity, int64_t timestamp, const Settings& settings)
{
	int sym_index;
	if (quantity <= 0
     || price <= 0
	 || (sym_index = settings.getIndexFromSymbol(symbol)) < 0
	   )
	{
		if (settings.verbose_level >= 1)
			spdlog::warn("Invalid trade: s={} p={} q={} T={}", symbol, price, quantity, timestamp);
		++invalid_trades_counter;
		return;
	}
	if (settings.verbose_level >= 2)
		spdlog::info("Add trade s={} p={} q={} T={} to stream {}",
			symbol, price, quantity, timestamp, sym_index);

	bool trade_in_the_past = false; // save here to avoid mutex-locked access to invalid_trades_counter

	{
		std::lock_guard<std::mutex> lock(mutex_for_queues);
		last_wall_time_of_trade = settings.getWallTime();
		max_server_time_of_trade = std::max(max_server_time_of_trade, timestamp);

		if (timestamp < min_valid_timestamp)
			trade_in_the_past = true;
		else
		{
			if (timestamp >= max_valid_timestamp)
				appendWindowsCoveredTimestamp(timestamp, settings);

			int64_t index = (timestamp - min_valid_timestamp) / settings.window_ms;
			queue[index].streams[sym_index].addTrade(price, quantity);
		}
	}

	if (trade_in_the_past)
	{
		if (settings.verbose_level >= 1)
			spdlog::warn("Trade from past: {}", timestamp);
		++invalid_trades_counter;
	}
}

void Aggregator::flush(const Settings& settings)
{
	// time to flush?
	// no guards because last_wall_time_of_flush is for main thread only
	int64_t wall_time = settings.getWallTime();
	if (last_wall_time_of_flush + settings.flush_interval_ms > wall_time)
		return;

	last_wall_time_of_flush = wall_time;

	std::deque<Window> flush_queue;

	auto window_ms = settings.window_ms;

	// quick move windows from work queue to flush queue and release mutex
	{
		std::lock_guard<std::mutex> lock(mutex_for_queues);
		// long pause on the stock exchange? flush anything!
		if (wall_time - last_wall_time_of_trade > 2 * settings.flush_interval_ms)
		{
			if (flush_queue.empty())
				flush_queue.swap(queue);
			else
			{
				// TODO: std::make_move_iterator?
				while (!queue.empty())
				{
					flush_queue.push_back(std::move(queue.front()));
					queue.pop_front();
				}
			}
		}
		else
		{
			// windows before max_server_time_of_trade are "in the past"
			// also may add some safety_gap_ms if it is allowed
			// flush windows until window with max_server_time_of_trade
			int64_t critical_time = max_server_time_of_trade - settings.safety_gap_ms;
			while (!queue.empty())
			{
				auto& window = queue.front();
				if (window.window_start_ms + window_ms > critical_time)
					break;
				flush_queue.push_back(std::move(window));
				queue.pop_front();
			}
		}
		if (queue.empty())
		{
			max_server_time_of_trade = 0;
			min_valid_timestamp = 0;
			max_valid_timestamp = 0;
		}
		else
		{
			min_valid_timestamp = queue.front().window_start_ms;
			max_valid_timestamp = queue.back().window_start_ms + window_ms;
		}
	}
	
	// write to disk
	while (!flush_queue.empty())
	{
		flush_queue.front().dump(file, settings);
		flush_queue.pop_front();
	}
	file.flush();
}

void Aggregator::finalFlush(const Settings& settings)
{
	std::lock_guard<std::mutex> lock(mutex_for_queues); // not necessary, just for case
	while (!queue.empty())
	{
		queue.front().dump(file, settings);
		queue.pop_front();
	}
	file.flush();
	max_server_time_of_trade = 0;
	min_valid_timestamp = 0;
	max_valid_timestamp = 0;
}

