#include "BinanceProvider.hpp"

#include "../../common/Logger.hpp"

BinanceProvider::BinanceProvider(std::shared_ptr<IHttpClient> client,
                                    const std::string& name,
                                    const std::string& api_key,
                                    const std::string& base_url,
                                    const Limits& limits,
                                    const std::vector<std::string>& config_caps) 
    : BaseProvider(client, api_key, base_url, limits)
    , name_(name) {
        for (const auto& s : config_caps) {
            auto crypto_cap = StringToCryptoCapability(s);
            if (crypto_cap.has_value()) {
                crypto_caps_.insert(crypto_cap.value());
            } else {
                Logger::Warn(
                    "[Binance] Unknown capability '{}' in config'", 
                    s);
            }
        }
    }

std::string BinanceProvider::GetName() const { 
    return name_; 
}

bool BinanceProvider::HasCapability(const CryptoCapability cap) const {
    return crypto_caps_.contains(cap);
}

boost::asio::awaitable<std::optional<double>> BinanceProvider::GetCryptoPrice(
        const std::string& ticker) {
    if (!HasCapability(CryptoCapability::RealtimePrice)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformGetAsync("/ticker/price", {{"symbol", ticker}});
    
    if (!json_opt.has_value()) {
        co_return std::nullopt;
    }

    std::string price_str = (json_opt.value()).value("price", "");
    double price = ParseDoubleSafe(price_str);
    
    if (price == 0.0) {
        co_return std::nullopt;
    }
    co_return price;
}

boost::asio::awaitable<std::optional<CryptoAsset>> BinanceProvider::GetCryptoAssetInfo(
        const std::string& ticker) {
    if (!HasCapability(CryptoCapability::Metadata)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformGetAsync("/exchangeInfo", {{"symbol", ticker}});
    
    if (!json_opt.has_value() || !json_opt->contains("symbols")) {
        co_return std::nullopt;
    }

    try {
        const auto& symbols = (json_opt.value())["symbols"];
        if (symbols.empty()) {
            co_return std::nullopt;
        }

        const auto& info = symbols[0];
        
        CryptoAsset asset;
        asset.id = -1;
        asset.ticker = info.value("symbol", "");
        asset.name = info.value("baseAsset", ""); 

        co_return asset;
    } catch (const std::exception& ex) {
        Logger::Error(
            "[Binance] Parse AssetInfo error for {}: {}", 
            ticker, ex.what());
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::vector<CryptoAsset>> BinanceProvider::GetCryptoTopList(const int limit) {
    if (!HasCapability(CryptoCapability::TopList)) {
        co_return std::vector<CryptoAsset>{};
    }

    auto json_opt = co_await PerformGetAsync("/ticker/24hr");
    
    if (!json_opt.has_value() || !json_opt->is_array()) {
        co_return std::vector<CryptoAsset>{};
    }

    std::vector<std::pair<double, CryptoAsset>> sorted_assets;

    const auto& data = json_opt.value();

    sorted_assets.reserve(data.size());

    for (const auto& item : data) {
        try {
            std::string symbol = item.value("symbol", "");

            if (symbol.length() <= 4 || symbol.substr(symbol.length() - 4) != "USDT") {
                continue;
            }

            double volume = ParseDoubleSafe(item.value("quoteVolume", "0"));

            CryptoAsset asset;
            asset.ticker = symbol;
            asset.name = symbol; 
            
            sorted_assets.push_back({volume, asset});
        } catch (const std::exception& ex) {
            Logger::Warn(
                "[Binance] Failed to parse item in TopList: {}",
                ex.what());
            continue; 
        }
    }

    std::sort(sorted_assets.begin(), sorted_assets.end(), [](const auto& a, const auto& b) { 
        return a.first > b.first; 
    });

    std::vector<CryptoAsset> results;
    results.reserve(std::min((size_t)limit, sorted_assets.size()));
    for (size_t i = 0; i < sorted_assets.size() && i < (size_t)limit; ++i) {
        results.push_back(sorted_assets[i].second);
    }
    
    co_return results;
}

boost::asio::awaitable<std::optional<OrderBook>> BinanceProvider::GetOrderBook(
        const std::string& ticker, 
        const int depth) {
    if (!HasCapability(CryptoCapability::OrderBook)) {
        co_return std::nullopt;
    }

    int limit = (depth <= 5) ? 5 : (depth <= 10) ? 10 : (depth <= 20) ? 20 : 50;

    auto json_opt = co_await PerformGetAsync("/depth", {
                                {"symbol", ticker}, 
                                {"limit", std::to_string(limit)}
                            });

    if (!json_opt.has_value()) {
        co_return std::nullopt;
    }

    try {
        OrderBook book;
        book.ticker = ticker;

        auto parse_entries = [this](const nlohmann::json& arr) {
            std::vector<OrderBookEntry> entries;
            if (!arr.is_array()) {
                return entries;
            }
            
            entries.reserve(arr.size());
            
            for (const auto& level : arr) {
                if (level.is_array() && level.size() >= 2) {
                    entries.push_back({
                        ParseDoubleSafe(level[0].get<std::string>()),
                        ParseDoubleSafe(level[1].get<std::string>()) 
                    });
                }
            }
            return entries;
        };

        if ((json_opt.value()).contains("bids")) {
            book.bids = parse_entries((json_opt.value())["bids"]);
        }
        if ((json_opt.value()).contains("asks")) {
            book.asks = parse_entries((json_opt.value())["asks"]);
        }

        co_return book;
    } catch (const std::exception& ex) {
        Logger::Error(
            "[Binance] OrderBook parse error for {}: {}", 
            ticker, ex.what());
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::vector<CryptoPriceCandle>> BinanceProvider::GetCryptoHistory(
        const std::string& ticker, 
        const Timestamp from, 
        const Timestamp to, 
        const TimeFrame interval) {
    if (!HasCapability(CryptoCapability::History)) {
        co_return std::vector<CryptoPriceCandle>{};
    }

    std::string interval_str = ConvertInterval(interval);

    if (interval_str.empty()) {
        Logger::Warn(
            "[Binance] Interval not supported: {}", 
            (int)interval);
        co_return std::vector<CryptoPriceCandle>{};
    }

    long long start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(from.time_since_epoch()).count();
    long long end_ms = std::chrono::duration_cast<std::chrono::milliseconds>(to.time_since_epoch()).count();

    auto json_opt = co_await PerformGetAsync("/klines", {
                                {"symbol", ticker},
                                {"interval", interval_str},
                                {"startTime", std::to_string(start_ms)},
                                {"endTime", std::to_string(end_ms)},
                                {"limit", "500"} 
                            });

    if (!json_opt.has_value() || !json_opt->is_array()) {
        co_return std::vector<CryptoPriceCandle>{};
    }

    std::vector<CryptoPriceCandle> results;
    try {
        const auto& data = json_opt.value();
        
        results.reserve(data.size());

        for (const auto& el : data) {
            if (!el.is_array() || el.size() < 6) {
                continue;
            }

            CryptoPriceCandle c;
            long long ts_ms = el[0].get<long long>();
            c.timestamp = std::chrono::system_clock::time_point(std::chrono::milliseconds(ts_ms));
            c.open = ParseDoubleSafe(el[1].get<std::string>());
            c.high = ParseDoubleSafe(el[2].get<std::string>());
            c.low = ParseDoubleSafe(el[3].get<std::string>());
            c.close = ParseDoubleSafe(el[4].get<std::string>());
            c.volume = ParseDoubleSafe(el[5].get<std::string>());

            results.push_back(c);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[Binance] History parse error for {}: {}", 
            ticker, ex.what());
    }
    co_return results;
}

boost::asio::awaitable<std::vector<CryptoAsset>> BinanceProvider::SearchAsset(const std::string& query) {
    co_return std::vector<CryptoAsset>{}; // ToDo: expand the enum to ensure strict compliance with the requested resource
}

boost::asio::awaitable<std::optional<GlobalCryptoMetrics>> BinanceProvider::GetGlobalMetrics() {
    if (HasCapability(CryptoCapability::GlobalMetrics)) {
        Logger::Warn(
            "[Binance] GlobalMetrics not supported directly");
    }
    co_return std::nullopt;
}

std::string BinanceProvider::ConvertInterval(const TimeFrame tf) {
    switch (tf) {
        case TimeFrame::Minute1: 
            return "1m";
        case TimeFrame::Minute5: 
            return "5m";
        case TimeFrame::Minute15: 
            return "15m";
        case TimeFrame::Hourly: 
            return "1h";
        case TimeFrame::Hour2: 
            return "2h";  
        case TimeFrame::Hour6: 
            return "6h";  
        case TimeFrame::Hour12: 
            return "12h"; 
        case TimeFrame::Daily: 
            return "1d";
        case TimeFrame::Weekly: 
            return "1w";
        case TimeFrame::Monthly: 
            return "1M";
        default: 
            throw std::runtime_error("Interval not supported by Binance");
    }
}