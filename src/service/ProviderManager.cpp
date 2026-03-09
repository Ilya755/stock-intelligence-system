#include "ProviderManager.hpp"

#include "../common/Logger.hpp"
#include "../http/CprHttpClient.hpp"
#include "../providers/impl/AlphaVantageProvider.hpp"
#include "../providers/impl/BinanceProvider.hpp"
#include "../providers/impl/CoinCapProvider.hpp"
#include "../providers/impl/CoinGeckoProvider.hpp"
#include "../providers/impl/FinnhubProvider.hpp"
#include "../providers/impl/FmpProvider.hpp" 
#include "../providers/impl/TwelveDataProvider.hpp"

ProviderManager& ProviderManager::GetInstance() {
    static ProviderManager instance;
    return instance;
}

void ProviderManager::Init(const Config& config) {
    Logger::Info("[ProviderManager] Initializing ProviderManager...");
    
    if (!http_client_) {
        http_client_ = std::make_shared<CprHttpClient>();
    }

    stock_providers_.clear();
    crypto_providers_.clear();

    const auto& api_configs = config.GetAllProviders();

    for (const auto& [key, cfg] : api_configs) {
        if (!cfg.enabled) {
            Logger::Info("[ProviderManager] Provider '{}' is disabled in config. Skipping.", key);
            continue;
        }

        try {
            if (key == "alpha_vantage") {
                Logger::Debug("[ProviderManager] Connecting AlphaVantage Provider...");

                auto p = std::make_shared<AlphaVantageProvider>(http_client_, key, cfg.api_key, cfg.base_url, 
                                                                    cfg.limits, cfg.capabilities);
                AddProvider(p, cfg.priority);
            } else if (key == "binance") {
                Logger::Debug("[ProviderManager] Connecting Binance Provider...");

                auto p = std::make_shared<BinanceProvider>(http_client_, key, cfg.api_key, cfg.base_url, 
                                                                cfg.limits, cfg.capabilities);
                AddProvider(p, cfg.priority);
            } else if (key == "coincap") {
                Logger::Debug("[ProviderManager] Connecting CoinCap Provider...");

                auto p = std::make_shared<CoinCapProvider>(http_client_, key, cfg.api_key, cfg.base_url, 
                                                                cfg.limits, cfg.capabilities);
                AddProvider(p, cfg.priority);
            } else if (key == "coingecko") {
                Logger::Debug("[ProviderManager] Connecting CoinGecko Provider...");

                auto p = std::make_shared<CoinGeckoProvider>(http_client_, key, cfg.api_key, cfg.base_url, 
                                                                cfg.limits, cfg.capabilities);
                AddProvider(p, cfg.priority);
            } else if (key == "finnhub") {
                Logger::Debug("[ProviderManager] Connecting Finnhub Provider...");

                auto p = std::make_shared<FinnhubProvider>(http_client_, key, cfg.api_key, cfg.base_url, 
                                                                cfg.limits, cfg.capabilities);
                AddProvider(p, cfg.priority);
            } else if (key == "financial_modeling_prep") {
                Logger::Debug("[ProviderManager] Connecting FMP Provider...");

                auto p = std::make_shared<FmpProvider>(http_client_, key, cfg.api_key, cfg.base_url, 
                                                            cfg.limits, cfg.capabilities);
                AddProvider(p, cfg.priority);
            } else if (key == "twelve_data") {
                Logger::Debug("[ProviderManager] Connecting TwelveData Provider...");

                auto p = std::make_shared<TwelveDataProvider>(http_client_, key, cfg.api_key, cfg.base_url, 
                                                                cfg.limits, cfg.capabilities);
                AddProvider(p, cfg.priority);
            } else {
                Logger::Warn("Unknown provider in config: '{}'.", key);
            }
        } catch (const std::exception& ex) {
            Logger::Error("Failed to initialize provider '{}': {}", key, ex.what());
        }
    }

    auto sort_providers = [](const auto& a, const auto& b) {
        return a.priority < b.priority;
    };

    std::sort(stock_providers_.begin(), stock_providers_.end(), sort_providers);
    std::sort(crypto_providers_.begin(), crypto_providers_.end(), sort_providers);

    {
        size_t stock_providers_count = stock_providers_.size();
        std::stringstream ss_stock;
        for (size_t i = 0; i < stock_providers_.size(); ++i) {
            ss_stock << stock_providers_[i].provider->GetName();
            if (i != stock_providers_.size() - 1) {
                ss_stock << ", ";
            }
        }

        size_t crypto_providers_count = crypto_providers_.size();
        std::stringstream ss_crypto;
        for (size_t i = 0; i < crypto_providers_count; ++i) {
            ss_crypto << crypto_providers_[i].provider->GetName();
            if (i != crypto_providers_count - 1) {
                ss_crypto << ", ";
            }
        }

        Logger::Info("[ProviderManager] Initialized. Stocks: [{}], Crypto: [{}]", ss_stock.str(), ss_crypto.str());
    }
}

std::optional<double> ProviderManager::GetStockPrice(const std::string& ticker) {
    return ExecuteStockRequest<double>(ProviderCapability::PriceRealtime,
                                    [&](auto p) { return p->GetStockPrice(ticker); },
                                    "GetStockPrice", ticker);
}

std::vector<StockPriceCandle> ProviderManager::GetStockHistory(const std::string& ticker, 
                                                                Timestamp from, Timestamp to, 
                                                                TimeFrame interval) {
    ProviderCapability cap = (interval == TimeFrame::Daily) ? ProviderCapability::PriceDaily : 
                                                                ProviderCapability::PriceIntraday;

    if (interval == TimeFrame::Weekly || interval == TimeFrame::Monthly) {
        cap = ProviderCapability::PriceHistoryDeep;
    }

    return ExecuteStockRequestList<StockPriceCandle>(cap,
                                    [&](auto p) { return p->GetStockHistory(ticker, from, to, interval); },
                                    "GetStockHistory", ticker);
}

std::optional<CompanyFullInfo> ProviderManager::GetCompanyProfile(const std::string& ticker) {
    return ExecuteStockRequest<CompanyFullInfo>(ProviderCapability::CompanyProfile,
                                    [&](auto p) { return p->GetCompanyProfile(ticker); },
                                    "GetCompanyProfile", ticker);
}

std::vector<TickerSearchResult> ProviderManager::SearchStockTicker(const std::string& query) {
    for (const auto& p : stock_providers_) {
        auto result = p.provider->SearchStockTicker(query);
        if (!result.empty()) {
            return result;
        }
    }
    return {};
}

std::vector<CompanyFinancialReport> ProviderManager::GetFinancialReports(const std::string& ticker) {
    return ExecuteStockRequestList<CompanyFinancialReport>(ProviderCapability::FinancialsDeep,
                                    [&](auto p) { return p->GetFinancialReports(ticker); },
                                    "GetFinancialReports", ticker);
}

std::vector<StockDividends> ProviderManager::GetDividends(const std::string& ticker, Date from, Date to) {
    return ExecuteStockRequestList<StockDividends>(ProviderCapability::Dividends,
                                    [&](auto p) { return p->GetDividends(ticker, from, to); },
                                    "GetDividends", ticker);
}

std::optional<double> ProviderManager::GetCryptoPrice(const std::string& ticker) {
    return ExecuteCryptoRequest<double>(CryptoCapability::RealtimePrice,
                                    [&](auto p) { return p->GetCryptoPrice(ticker); },
                                    "GetCryptoPrice", ticker);
}

std::vector<CryptoPriceCandle> ProviderManager::GetCryptoHistory(const std::string& ticker, 
                                                                    Timestamp from, Timestamp to, 
                                                                    TimeFrame interval) {
    return ExecuteCryptoRequestList<CryptoPriceCandle>(CryptoCapability::History,
                                    [&](auto p) { return p->GetCryptoHistory(ticker, from, to, interval); },
                                    "GetCryptoHistory", ticker);
}

std::optional<CryptoAsset> ProviderManager::GetCryptoAssetInfo(const std::string& ticker) {
    return ExecuteCryptoRequest<CryptoAsset>(CryptoCapability::Metadata,
                                    [&](auto p) { return p->GetCryptoAssetInfo(ticker); },
                                    "GetCryptoAssetInfo", ticker);
}

std::vector<CryptoAsset> ProviderManager::GetCryptoTopList(int limit) {
    return ExecuteCryptoRequestList<CryptoAsset>(CryptoCapability::TopList,
                                    [&](auto p) { return p->GetCryptoTopList(limit); },
                                    "GetCryptoTopList", "TOP");
}

std::optional<GlobalCryptoMetrics> ProviderManager::GetGlobalCryptoMetrics() {
    return ExecuteCryptoRequest<GlobalCryptoMetrics>(CryptoCapability::GlobalMetrics,
                                    [&](auto p) { return p->GetGlobalMetrics(); },
                                    "GetGlobalCryptoMetrics", "GLOBAL");
}

std::optional<OrderBook> ProviderManager::GetOrderBook(const std::string& ticker, int depth) {
    return ExecuteCryptoRequest<OrderBook>(CryptoCapability::OrderBook,
                                    [&](auto p) { return p->GetOrderBook(ticker, depth); },
                                    "GetOrderBook", ticker);
}

template <typename T>
void ProviderManager::AddProvider(std::shared_ptr<T> provider, int priority) {
    if (std::shared_ptr<IStockProvider> sp = std::dynamic_pointer_cast<IStockProvider>(provider)) {
        stock_providers_.push_back({sp, priority});
    }
    
    if (std::shared_ptr<ICryptoProvider> cp = std::dynamic_pointer_cast<ICryptoProvider>(provider)) {
        crypto_providers_.push_back({cp, priority});
    }
}

template <typename ResultType, typename Func>
std::optional<ResultType> ProviderManager::ExecuteStockRequest(ProviderCapability cap, Func action, 
                                                                const std::string& action_name, 
                                                                const std::string& context) {
    for (const auto& p : stock_providers_) {
        if (p.provider->HasCapability(cap)) {
            Logger::Debug("[ProviderManager] Using {} for {} ({})", p.provider->GetName(), action_name, context);
            
            try{
                auto result = action(p.provider);
                if (result.has_value()) {
                    return result.value();
                }
                Logger::Warn("[ProviderManager] {} failed with {}. Trying next...", 
                                action_name, p.provider->GetName());
            } catch (const std::exception& ex) {
                Logger::Warn("[ProviderManager] {} threw an exception: {}. Trying next...", 
                                p.provider->GetName(), ex.what());
            }
        }
    }
    Logger::Error("[ProviderManager] All providers failed for {} ({})", action_name, context);
    return std::nullopt;
}

template <typename ResultItem, typename Func>
std::vector<ResultItem> ProviderManager::ExecuteStockRequestList(ProviderCapability cap, Func action, 
                                                                    const std::string& action_name, 
                                                                    const std::string& context) {
    for (const auto& p : stock_providers_) {
        if (p.provider->HasCapability(cap)) {
            Logger::Debug("[ProviderManager] Using {} for {} ({})", p.provider->GetName(), action_name, context);
            
            try {
                auto result = action(p.provider);
                if (!result.empty()) {
                    return result;
                }
                Logger::Warn("[ProviderManager] {} failed with {}. Trying next...", 
                                action_name, p.provider->GetName());
            } catch (const std::exception& ex) {
                Logger::Warn("[ProviderManager] {} threw an exception: {}. Trying next...", 
                                p.provider->GetName(), ex.what());
            }
        }
    }
    Logger::Error("[ProviderManager] All providers failed or returned empty for {} ({})", action_name, context);
    return {};
}

template <typename ResultType, typename Func>
std::optional<ResultType> ProviderManager::ExecuteCryptoRequest(CryptoCapability cap, Func action, 
                                                                const std::string& action_name, 
                                                                const std::string& context) {
    for (const auto& p : crypto_providers_) {
        if (p.provider->HasCapability(cap)) {
            Logger::Debug("[ProviderManager] Using {} for {} ({})", p.provider->GetName(), action_name, context);
            
            try {
                auto result = action(p.provider);
                if (result.has_value()) {
                    return result.value();
                }
                Logger::Warn("[ProviderManager] {} failed with {}. Trying next...", 
                                action_name, p.provider->GetName());
            } catch (const std::exception& ex) {
                Logger::Warn("[ProviderManager] {} threw an exception: {}. Trying next...", 
                                p.provider->GetName(), ex.what());
            }
        }
    }
    Logger::Error("[ProviderManager] All crypto providers failed for {} ({})", action_name, context);
    return std::nullopt;
}

template <typename ResultItem, typename Func>
std::vector<ResultItem> ProviderManager::ExecuteCryptoRequestList(CryptoCapability cap, Func action, 
                                                                    const std::string& action_name, 
                                                                    const std::string& context) {
    for (const auto& p : crypto_providers_) {
        if (p.provider->HasCapability(cap)) {
            Logger::Debug("[ProviderManager] Using {} for {} ({})", p.provider->GetName(), action_name, context);
            
            try {
                auto result = action(p.provider);
                if (!result.empty()) {
                    return result;
                }
                Logger::Warn("[ProviderManager] {} failed with {}. Trying next...", 
                                action_name, p.provider->GetName());
            } catch (const std::exception& ex) {
                Logger::Warn("[ProviderManager] {} threw an exception: {}. Trying next...", 
                                p.provider->GetName(), ex.what());
            }
        }
    }
    Logger::Error("[ProviderManager] All crypto providers failed for {} ({})", action_name, context);
    return {};
}