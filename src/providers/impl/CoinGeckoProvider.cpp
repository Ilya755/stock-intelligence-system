#include "CoinGeckoProvider.hpp"

#include "../../common/Logger.hpp"

CoinGeckoProvider::CoinGeckoProvider(std::shared_ptr<IHttpClient> client,
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
                    "[CoinGecko] Unknown capability '{}' in config", 
                    s);
            }
        }
    }

std::string CoinGeckoProvider::GetName() const { 
    return name_; 
}

bool CoinGeckoProvider::HasCapability(const CryptoCapability cap) const {
    return crypto_caps_.contains(cap);
}

boost::asio::awaitable<std::optional<double>> CoinGeckoProvider::GetCryptoPrice(
        const std::string& ticker) { // ticker - name of asset
    if (!HasCapability(CryptoCapability::RealtimePrice)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformGetAsync("/simple/price", {
                                {"ids", ticker},
                                {"vs_currencies", "usd"}
                            });
    
    if (!json_opt.has_value() || !json_opt->contains(ticker)) {
        co_return std::nullopt;
    }

    try {
        const auto& item = (json_opt.value())[ticker];
        if (item.contains("usd")) {
            co_return item["usd"].get<double>();
        }
        co_return std::nullopt;
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CoinGecko] Parse Price Error for {}: {}", 
            ticker, ex.what());
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::optional<CryptoAsset>> CoinGeckoProvider::GetCryptoAssetInfo(
        const std::string& ticker) {  // ticker - name of asset
    if (!HasCapability(CryptoCapability::Metadata)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformGetAsync("/coins/" + ticker, {
                                {"localization", "false"},
                                {"tickers", "false"},
                                {"market_data", "false"},
                                {"community_data", "false"},
                                {"developer_data", "false"}
                            });

    if (!json_opt.has_value() || json_opt->empty()) {
        co_return std::nullopt;
    }

    try {
        const auto& j = json_opt.value();

        CryptoAsset asset;
        asset.id = -1; 
        asset.ticker = j.value("symbol", ""); 
        asset.name = j.value("name", "");
        
        co_return asset;
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CoinGecko] Parse AssetInfo Error for {}: {}", 
            ticker, ex.what());
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::vector<CryptoAsset>> CoinGeckoProvider::GetCryptoTopList(const int limit) {
    if (!HasCapability(CryptoCapability::TopList)) {
        co_return std::vector<CryptoAsset>{};
    }

    auto json_opt = co_await PerformGetAsync("/coins/markets", {
                                {"vs_currency", "usd"},
                                {"order", "market_cap_desc"},
                                {"per_page", std::to_string(limit)},
                                {"page", "1"},
                                {"sparkline", "false"}
                            });
    
    if (!json_opt.has_value() || !json_opt->is_array()) {
        co_return std::vector<CryptoAsset>{};
    }

    std::vector<CryptoAsset> results;
    try {
        const auto& data = json_opt.value();
    
        results.reserve(data.size());

        for (const auto& item : data) {
            CryptoAsset asset;
            asset.ticker = item.value("symbol", "");
            asset.name = item.value("name", "");

            results.push_back(asset);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CoinGecko] TopList Parse Error: {}", 
            ex.what());
    }
    co_return results;
}

boost::asio::awaitable<std::vector<CryptoPriceCandle>> CoinGeckoProvider::GetCryptoHistory(
        const std::string& ticker, 
        const Timestamp from, 
        const Timestamp to, 
        const TimeFrame interval) {
    if (!HasCapability(CryptoCapability::History)) {
        co_return std::vector<CryptoPriceCandle>{};
    }

    long long from_ts = std::chrono::duration_cast<std::chrono::seconds>(from.time_since_epoch()).count();
    long long to_ts = std::chrono::duration_cast<std::chrono::seconds>(to.time_since_epoch()).count();

    auto json_opt = co_await PerformGetAsync("/coins/" + ticker + "/market_chart/range", {
                                    {"vs_currency", "usd"},
                                    {"from", std::to_string(from_ts)},
                                    {"to", std::to_string(to_ts)}
                                });

    if (!json_opt.has_value() || !json_opt->contains("prices")) {
        co_return std::vector<CryptoPriceCandle>{};
    }

    std::vector<CryptoPriceCandle> results;
    try {
        const auto& prices = (json_opt.value())["prices"];

        const auto& volumes = (json_opt.value()).contains("total_volumes") ? 
                                    (json_opt.value())["total_volumes"] : nlohmann::json::array();

        const size_t count = prices.size();

        results.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            const auto& price_point = prices[i];
            if (!price_point.is_array() || price_point.size() < 2) {
                continue;
            }

            CryptoPriceCandle c;

            long long ts_ms = static_cast<long long>(price_point[0].get<double>());
            c.timestamp = std::chrono::system_clock::time_point(std::chrono::milliseconds(ts_ms));
            
            double price = price_point[1].get<double>();
            
            c.open = price;
            c.high = price;
            c.low = price;
            c.close = price;

            c.volume = 0.0;
            if (i < volumes.size() && volumes[i].is_array() && volumes[i].size() >= 2) {
                c.volume = volumes[i][1].get<double>();
            }

            results.push_back(c);
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CoinGecko] History Parse Error for {}: {}",
            ticker, ex.what());
    }
    co_return results;
}

boost::asio::awaitable<std::vector<CryptoAsset>> CoinGeckoProvider::SearchAsset(const std::string& query) {
    if (!HasCapability(CryptoCapability::Metadata) && !HasCapability(CryptoCapability::TopList)) {
        co_return std::vector<CryptoAsset>{};
    }

    auto json_opt = co_await PerformGetAsync("/search", {{"query", query}});
    
    if (!json_opt.has_value() || !json_opt->contains("coins")) {
        co_return std::vector<CryptoAsset>{};
    }

    std::vector<CryptoAsset> results;
    try {
        const auto& coins = (json_opt.value())["coins"];
        
        if (!coins.is_array()) {
            co_return std::vector<CryptoAsset>{};
        }

        results.reserve(coins.size());

        for (const auto& item : coins) {
            CryptoAsset asset;
            asset.ticker = item.value("symbol", "");
            asset.name = item.value("name", "");
            
            results.push_back(asset);    
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CoinGecko] Search Parse Error: {}", 
            ex.what());
    }
    co_return results;
}


boost::asio::awaitable<std::optional<GlobalCryptoMetrics>> CoinGeckoProvider::GetGlobalMetrics() {
    if (!HasCapability(CryptoCapability::GlobalMetrics)) {
        co_return std::nullopt;
    }

    auto json_opt = co_await PerformGetAsync("/global");
    
    if (!json_opt.has_value() || !json_opt->contains("data")) {
        co_return std::nullopt;
    }

    try {
        const auto& data = (json_opt.value())["data"];
        GlobalCryptoMetrics m;
        
        if (data.contains("total_market_cap") && data["total_market_cap"].contains("usd")) {
            m.total_market_cap_usd = data["total_market_cap"]["usd"].get<double>();
        } else {
            m.total_market_cap_usd = 0.0;
        }

        if (data.contains("total_volume") && data["total_volume"].contains("usd")) {
            m.total_volume_24h = data["total_volume"]["usd"].get<double>();
        } else {
            m.total_volume_24h = 0.0;
        }

        if (data.contains("market_cap_percentage") && data["market_cap_percentage"].contains("btc")) {
            m.btc_dominance_percentage = data["market_cap_percentage"]["btc"].get<double>();
        } else {
            m.btc_dominance_percentage = 0.0;
        }

        m.active_cryptocurrencies = data.value("active_cryptocurrencies", 0);

        co_return m;
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CoinGecko] GlobalMetrics Parse Error: {}", 
            ex.what());
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::optional<OrderBook>> CoinGeckoProvider::GetOrderBook(const std::string&, const int) {
    if (HasCapability(CryptoCapability::OrderBook)) {
        Logger::Warn(
            "[CoinGecko] OrderBook capability is set but not supported via REST API");
    }
    co_return std::nullopt;
}