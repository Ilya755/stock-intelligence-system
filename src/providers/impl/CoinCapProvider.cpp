#include "CoinCapProvider.hpp"

#include "../../common/Logger.hpp"

CoinCapProvider::CoinCapProvider(std::shared_ptr<IHttpClient> client,
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
                    "[CoinCap] Unknown capability '{}' in config", 
                    s);
            }
        }
    }

std::string CoinCapProvider::GetName() const { 
    return name_; 
}

bool CoinCapProvider::HasCapability(const CryptoCapability cap) const {
    return crypto_caps_.contains(cap);
}

boost::asio::awaitable<std::optional<double>> CoinCapProvider::GetCryptoPrice(const std::string& ticker) {
    if (!HasCapability(CryptoCapability::RealtimePrice)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformGetAsync("/assets/" + ticker);
    
    if (!json_opt.has_value() || !json_opt->contains("data")) {
        co_return std::nullopt;
    }
    const auto& data = (json_opt.value())["data"];
    double price = ParseDoubleSafe(data.value("priceUsd", ""));
    if (price == 0.0) {
        co_return std::nullopt;
    }
    co_return price;
}

boost::asio::awaitable<std::optional<CryptoAsset>> CoinCapProvider::GetCryptoAssetInfo(const std::string& ticker) {
    if (!HasCapability(CryptoCapability::Metadata)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformGetAsync("/assets/" + ticker);
    
    if (!json_opt.has_value() || !json_opt->contains("data")) {
        co_return std::nullopt;
    }

    try {
        const auto& data = (json_opt.value())["data"];

        CryptoAsset asset;
        asset.id = -1;
        asset.ticker = data.value("symbol", ""); 
        asset.name = data.value("name", "");

        co_return asset;
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CoinCap] Parse AssetInfo error for {}: {}", 
            ticker, ex.what());
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::vector<CryptoAsset>> CoinCapProvider::GetCryptoTopList(const int limit) {
    if (!HasCapability(CryptoCapability::TopList)) {
        co_return std::vector<CryptoAsset>{};
    }

    auto json_opt = co_await PerformGetAsync("/assets", {{"limit", std::to_string(limit)}});
    
    if (!json_opt.has_value() || !json_opt->contains("data") || 
        !(json_opt.value())["data"].is_array()) {
        co_return std::vector<CryptoAsset>{};
    }

    std::vector<CryptoAsset> results;
    try {
        const auto& data = (json_opt.value())["data"];
        
        results.reserve(data.size());

        for (const auto& item : data) {
            CryptoAsset asset;
            asset.ticker = item.value("symbol", "");
            asset.name = item.value("name", "");

            results.push_back(asset);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CoinCap] TopList parse error: {}", 
            ex.what());
    }
    co_return results;
}

boost::asio::awaitable<std::vector<CryptoPriceCandle>> CoinCapProvider::GetCryptoHistory(const std::string& ticker, 
                                                                    const Timestamp from, const Timestamp to, 
                                                                    const TimeFrame interval) {
    if (!HasCapability(CryptoCapability::History)) {
        co_return std::vector<CryptoPriceCandle>{};
    }

    std::string interval_str = ConvertInterval(interval);

    long long start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(from.time_since_epoch()).count();
    long long end_ms = std::chrono::duration_cast<std::chrono::milliseconds>(to.time_since_epoch()).count();

    auto json_opt = co_await PerformGetAsync("/assets/" + ticker + "/history", {
                                {"interval", interval_str},
                                {"start", std::to_string(start_ms)},
                                {"end", std::to_string(end_ms)}
                            });

    if (!json_opt.has_value() || !json_opt->contains("data") || !(json_opt.value())["data"].is_array()) {
        co_return std::vector<CryptoPriceCandle>{};
    }

    std::vector<CryptoPriceCandle> results;
    try {
        const auto& data = (json_opt.value())["data"];

        results.reserve(data.size());

        for (const auto& item : data) {
            CryptoPriceCandle c;
            
            long long ts_ms = item.value("time", 0LL);
            c.timestamp = std::chrono::system_clock::time_point(std::chrono::milliseconds(ts_ms));
            
            double price = ParseDoubleSafe(item.value("priceUsd", ""));

            c.open = price;
            c.high = price;
            c.low = price;
            c.close = price;
            c.volume = 0.0;

            results.push_back(c);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CoinCap] History parse error for {}: {}", 
            ticker, ex.what());
    }
    co_return results;
}

boost::asio::awaitable<std::vector<CryptoAsset>> CoinCapProvider::SearchAsset(const std::string& query) {
    if (!HasCapability(CryptoCapability::TopList)) {
        co_return std::vector<CryptoAsset>{};
    }

    auto json_opt = co_await PerformGetAsync("/assets", {{"search", query}, {"limit", "10"}});
    
    if (!json_opt.has_value() || !json_opt->contains("data")) {
        co_return std::vector<CryptoAsset>{};
    }

    std::vector<CryptoAsset> results;
    try {
        const auto& data = (json_opt.value())["data"];
        
        if (!data.is_array()) {
            co_return std::vector<CryptoAsset>{};
        }

        results.reserve(data.size());

        for (const auto& item : data) {  
            CryptoAsset asset;
            asset.ticker = item.value("symbol", "");
            asset.name = item.value("name", ""); 

            results.push_back(asset);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CoinCap] Search parse error: {}", 
            ex.what());
    }
    co_return results;
}

boost::asio::awaitable<std::optional<GlobalCryptoMetrics>> CoinCapProvider::GetGlobalMetrics() {
    if (HasCapability(CryptoCapability::GlobalMetrics)) {
        Logger::Warn(
            "[CoinCap] GlobalMetrics requested but not natively supported by API v2");
    }
    co_return std::nullopt;
}

boost::asio::awaitable<std::optional<OrderBook>> CoinCapProvider::GetOrderBook(const std::string&, int) {
    if (HasCapability(CryptoCapability::OrderBook)) {
        Logger::Warn(
            "[CoinCap] OrderBook requested but only available via WebSocket");
    }
    co_return std::nullopt;
}

std::string CoinCapProvider::ConvertInterval(const TimeFrame tf) {
    switch (tf) {
        case TimeFrame::Minute1: 
            return "m1";
        case TimeFrame::Minute5: 
            return "m5";
        case TimeFrame::Minute15: 
            return "m15";
        case TimeFrame::Hourly: 
            return "h1";
        case TimeFrame::Daily: 
            return "d1";
        case TimeFrame::Hour2: 
            return "h2";
        case TimeFrame::Hour6: 
            return "h6";
        case TimeFrame::Hour12: 
            return "h12";
        default: 
            throw std::runtime_error("Interval not supported by CoinCap");
    }
}