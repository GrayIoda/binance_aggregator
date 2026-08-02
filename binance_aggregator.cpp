// Main module
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXNetSystem.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <filesystem>
#include <csignal>
#include <string_view>
#include "settings.h"
#include "aggregator.h"
#include "jsonparser.h"

namespace fs = std::filesystem;

// Linux-specific signal handling
static void setup_signal_mask(sigset_t& sigset) 
{
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);  // Ctrl+C
    sigaddset(&sigset, SIGTERM); // polite kill
    pthread_sigmask(SIG_BLOCK, &sigset, nullptr);
}

int main(int argc, char **argv) 
{
    // return value
    int ret = 0;

    // hint for user
    if (argc > 1 && std::string_view(argv[1]) == "--help")
    {
        std::cout << "Format: binance_aggregator [<ini-file>]" << std::endl;
        return 0;
    }

    // optional for linux
    ix::initNetSystem();

    try
    {
        fs::path config_path = "settings.ini";
        if (argc > 1)
            config_path = argv[1];

        Settings settings;
        settings.load(config_path);

        Aggregator aggr(settings);
        // time of last dumping
        int64_t dump_time = Settings::getEpochTime();
        aggr.moveInTime(dump_time, settings);
        
        ix::WebSocket ws;

        // init TLS
        ix::SocketTLSOptions tlsOptions;
        tlsOptions.caFile = "/etc/ssl/certs/ca-certificates.crt";
        ws.setTLSOptions(tlsOptions);

        ws.setUrl(settings.getURLForBinance());

        // to search streams by upper case
        settings.useUpperNames();

        // IXWebSocket standart callback behaviour
        ws.setOnMessageCallback(
            [&](const ix::WebSocketMessagePtr& msg) 
        {
            switch (msg->type)
            {
                case ix::WebSocketMessageType::Open:
                    spdlog::info("Connected to Binance");
                break;

                case ix::WebSocketMessageType::Message:
                    try
                    {
                        // need dump?
                        int64_t cur_time = Settings::getEpochTime();
                        if (cur_time >= dump_time + settings.flush_interval_ms)
                        {
                            // TODO: here should use server time (timestamp from trade) 
                            // if is it not too far from wall time
                            aggr.moveInTime(cur_time, settings);
                            dump_time = cur_time;
                        }

                        auto [symbol, price, quantity, timestamp] = parse_binanse_json(msg->str, settings);
                        aggr.addTrade(symbol, price, quantity, timestamp, settings);
                    }
                    catch (const std::exception &ex)
                    {
                        if (settings.verbose)
                            spdlog::error("Invalid JSON: {}", ex.what());
                        aggr.incrementErrors();
                    }
                break;

                case ix::WebSocketMessageType::Error:
                    spdlog::error("WebSocket error: {}", msg->errorInfo.reason);
                break;

                case ix::WebSocketMessageType::Close:
                    if (msg->closeInfo.code == ix::WebSocketCloseConstants::kNormalClosureCode)
                        spdlog::info("Disconnect from Binance");
                    else
                        spdlog::warn("WebSocket closed: code={} reason={}", msg->closeInfo.code, msg->closeInfo.reason);
                break;

            default:
                break;
            }
        });

        spdlog::info("Try to connect to Binance...");

        sigset_t sigset;
        setup_signal_mask(sigset);

        ws.start();

        // wait for ^C or kill
        int sig = 0;
        sigwait(&sigset, &sig);

        ws.stop();

        // graceful shutdown
        aggr.flushAll(settings);
    }
    catch (std::exception& ex)
    {
        // failure shutdown
        spdlog::error("Fatal error: {}", ex.what());
        ret = -1;
    }
    ix::uninitNetSystem();
    return ret;
}
