// Main module
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXNetSystem.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <filesystem>
#include <csignal>
#include <string_view>
#include <atomic>
#include "settings.h"
#include "aggregator.h"
#include "jsonparser.h"

namespace fs = std::filesystem;

// signal atomic
static std::atomic<bool> can_continue_flag { true };

void signal_handler(int sig) 
{
    can_continue_flag.store(false, std::memory_order_relaxed);
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

        ix::WebSocket ws;

        // init TLS
        ix::SocketTLSOptions tlsOptions;
        tlsOptions.caFile = "/etc/ssl/certs/ca-certificates.crt";
        ws.setTLSOptions(tlsOptions);

        ws.setUrl(settings.getURLForBinance());
        ws.setPingInterval(20); // avoid close socket as not active
        ws.setHandshakeTimeout(10); // avoid too long connection
        // ws.enableAutomaticReconnection(); // task do not want this

        // to search streams by upper case
        settings.useUpperNames();

        Aggregator aggr(settings);

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
                        auto [symbol, price, quantity, timestamp] = parse_binanse_json(msg->str, settings);
                        aggr.addTrade(symbol, price, quantity, timestamp, settings);
                    }
                    catch (const std::exception &ex)
                    {
                        if (settings.verbose_level >= 1)
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

        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        ws.start();

        // wait for ^C or kill
        while (can_continue_flag.load(std::memory_order_relaxed))
        {
            aggr.flush(settings);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        ws.stop();

        // graceful shutdown
        aggr.finalFlush(settings);
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
