#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <chrono>
#include <csignal>

#include "boost/asio.hpp"
#include "boost/asio/signal_set.hpp"

#include "common/Config.hpp"
#include "common/Logger.hpp"
#include "common/TimeUtils.hpp"

#include "db/Database.hpp"
#include "db/StockRepository.hpp"
#include "db/CryptoRepository.hpp"

#include "service/ProviderManager.hpp"
#include "service/MarketService.hpp"

#include "server/RequestHandler.hpp"
#include "server/TcpServer.hpp"

boost::asio::awaitable<void> RunSelfTest(std::shared_ptr<MarketService> service) {
    Logger::Info("==================================================");
    Logger::Info("           SYSTEM SELF-TEST SEQUENCE              ");
    Logger::Info("==================================================");

    std::string crypto_ticker = "bitcoin"; 
    Logger::Info(
        "[SelfTest] 1. Requesting Crypto Price for '{}'...", 
        crypto_ticker);
    
    auto btc_price = co_await service->GetCryptoPrice(crypto_ticker);
    if (btc_price.has_value()) {
        Logger::Info(
            "[SelfTest] [OK] Bitcoin Price: ${:.2f}", 
            *btc_price);
    } else {
        Logger::Error(
            "[SelfTest] [FAIL] Could not fetch Bitcoin price. Check internet or API limits.");
    }

    std::string eth_ticker = "ethereum";

    Logger::Info(
        "[SelfTest] 2. Requesting Crypto Metadata for '{}'...", 
        eth_ticker);

    auto eth_info = co_await service->GetCryptoAssetInfo(eth_ticker);
    if (eth_info.has_value()) {
        Logger::Info(
            "[SelfTest] [OK] Asset Found: ID={}, Name='{}', Ticker='{}'", 
            eth_info->id, eth_info->name, eth_info->ticker);
    } else {
        Logger::Warn(
            "[SelfTest] [WARN] Could not fetch Ethereum info.");
    }

    std::string stock_ticker = "AAPL";
    Logger::Info(
        "[SelfTest] 3. Requesting Stock Price for '{}'...", 
        stock_ticker);
    auto aapl_price = co_await service->GetStockPrice(stock_ticker);
    if (aapl_price.has_value()) {
        Logger::Info(
            "[SelfTest] [OK] Apple Price: ${:.2f}", 
            *aapl_price);
    } else {
        Logger::Warn(
            "[SelfTest] [WARN] Could not fetch AAPL price. (Providers might be disabled or limited)");
    }

    Logger::Info(
        "[SelfTest] 4. Requesting Crypto History (3 days)...");
    auto now = std::chrono::system_clock::now();
    auto start = now - std::chrono::hours(24 * 3);
    auto history = co_await service->GetCryptoHistory(crypto_ticker, start, now, TimeFrame::Daily);
    
    if (!history.empty()) {
        Logger::Info(
            "[SelfTest] [OK] Fetched {} candles. Last Close: {:.2f}", 
            history.size(), history.back().close);
    } else {
        Logger::Warn(
            "[SelfTest] [WARN] History is empty.");
    }

    Logger::Info(
        "[SelfTest] 5. Requesting Top 5 Crypto Assets...");
    auto top = co_await service->GetTopCryptoAssets(5);
    if (!top.empty()) {
        Logger::Info(
            "[SelfTest] [OK] Top #1 is '{}'", 
            top[0].name);
    } else {
        Logger::Warn(
            "[SelfTest] [WARN] Top list empty.");
    }

    Logger::Info("==================================================");
    Logger::Info("           SELF-TEST COMPLETE                     ");
    Logger::Info("==================================================");
}

int main() {
    std::setbuf(stdout, nullptr);
    std::setbuf(stderr, nullptr);

    spdlog::flush_on(spdlog::level::info);
    
    try {
        Logger::InitConsole();
        Logger::Info(
            "Starting Stock Intelligence System...");

        const auto& config = Config::GetInstance();
        boost::asio::io_context io_context;

        const std::string log_dir = config.GetLoggerPath();
        
        std::filesystem::path log_file_path = std::filesystem::path(log_dir) / "program.log";

        if (!std::filesystem::exists(log_dir)) {
            std::filesystem::create_directories(log_dir);
        }
        
        Logger::InitFile(log_file_path.string(), "debug");


        Logger::Info(
            "[Init] Establishing Database Connection Pool...");
        Database db(io_context.get_executor());

        auto stock_repo = std::make_shared<StockRepository>(db);
        auto crypto_repo = std::make_shared<CryptoRepository>(db);

        std::shared_ptr<MarketService> market_service;
        std::shared_ptr<RequestHandler> request_handler;
        std::unique_ptr<TcpServer> server;
        int exit_code = 0;

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int signal_number) {
            Logger::Info(
                "[System] Signal {} received. Shutting down...", 
                signal_number);

            io_context.stop();

            Logger::Info(
                "[System] Shutdown complete. Bye!");
        });


        boost::asio::co_spawn(
            io_context,
            [&]() -> boost::asio::awaitable<void> {
                Logger::Info(
                    "[Init] Establishing asynchronous Database Connection Pool...");
                co_await db.Initialize();

                Logger::Info(
                    "[Init] Configuring API Providers...");
                ProviderManager::GetInstance().Init(config, io_context.get_executor());

                market_service = std::make_shared<MarketService>(stock_repo, crypto_repo);
                request_handler = std::make_shared<RequestHandler>(market_service);

                const int port = config.GetServer().port;
                server = std::make_unique<TcpServer>(io_context, port, request_handler);

                boost::asio::co_spawn(
                    io_context,
                    RunSelfTest(market_service),
                    [](std::exception_ptr error) {
                        if (!error) {
                            return;
                        }
                        try {
                            std::rethrow_exception(error);
                        } catch (const std::exception& ex) {
                            Logger::Error(
                                "[SelfTest] Unexpected failure: {}", 
                                ex.what());
                        }
                    });

                Logger::Info("--------------------------------------------------");
                Logger::Info("Server is RUNNING on port {}", port);
                Logger::Info("Waiting for connections...");
                Logger::Info("--------------------------------------------------");
            },
            [&](std::exception_ptr error) {
                if (!error) {
                    return;
                }
                exit_code = 1;
                try {
                    std::rethrow_exception(error);
                } catch (const std::exception& ex) {
                    Logger::Critical(
                        "Application initialization failed: {}", 
                        ex.what());
                }
                io_context.stop();
            });

        io_context.run();

        return exit_code;
    } catch (const std::exception& ex) {
        Logger::Critical(
            "FATAL ERROR: {}", 
            ex.what());
        return 1;
    }

    return 0;
}