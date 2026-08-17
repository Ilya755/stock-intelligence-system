#include "MarketService.hpp"

#include "../common/Logger.hpp"

MarketService::MarketService(
        std::shared_ptr<StockRepository> stock_repo,
        std::shared_ptr<CryptoRepository> crypto_repo)
    : stock_repo_(stock_repo) 
    , crypto_repo_(crypto_repo)
    , provider_manager_(ProviderManager::GetInstance()) 
    {}

boost::asio::awaitable<std::optional<double>> MarketService::GetStockPrice(const std::string& ticker) {
    const int MAX_TIME_DIFF_MINUTES = 5;

    auto company_opt = co_await stock_repo_->GetCompanyByTickerAsync(ticker);
    
    if (company_opt.has_value()) {
        auto last_candle = co_await stock_repo_->GetLastStockPriceAsync(company_opt->id);

        if (last_candle.has_value()) {
            auto now = std::chrono::system_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::minutes>(now - last_candle->timestamp);
            
            if (diff.count() < MAX_TIME_DIFF_MINUTES) {
                Logger::Debug(
                    "[MarketService] Returning cached stock price for {}", 
                    ticker);
                co_return last_candle->close; 
            }
        }
    }

    Logger::Debug(
        "[MarketService] Fetching fresh stock price for {} from providers...", 
        ticker);

    auto price_opt = co_await provider_manager_.GetStockPrice(ticker);
    
    if (price_opt.has_value()) {
        auto company_id_opt = co_await EnsureCompanyExists(ticker);

        if (company_id_opt.has_value()) {
            StockPriceCandle candle;
            candle.timestamp = std::chrono::system_clock::now();
            candle.close = price_opt.value();
            candle.open = price_opt.value();
            candle.high = price_opt.value();
            candle.low = price_opt.value();
            candle.volume = 0; 

            co_await stock_repo_->SaveStockPriceAsync(company_id_opt.value(), candle);
            Logger::Debug(
                "[MarketService] Saved new Stock Price for {} to DB", 
                ticker);
        }
        co_return price_opt;
    }

    Logger::Warn(
        "[MarketService] Failed to get Stock Price for {}", 
        ticker);
    co_return std::nullopt;
}

boost::asio::awaitable<std::optional<CompanyFullInfo>> MarketService::GetCompanyProfile(
        const std::string& ticker) {
    const auto TIME_LIMIT = std::chrono::hours(24 * 7);

    auto db_profile = co_await stock_repo_->GetCompanyByTickerAsync(ticker);
    bool need_fetch = true;

    if (db_profile.has_value()) {
        auto age = std::chrono::system_clock::now() - db_profile->updated_at;

        if (age < TIME_LIMIT) {
            Logger::Debug(
                "[MarketService] Returning cached Company Profile for {}", 
                ticker);
            need_fetch = false;

            co_return db_profile;
        } else {
            Logger::Debug(
                "[MarketService] Company Profile for {} is stale. Refreshing...", 
                ticker);
        }
    }

    Logger::Debug(
        "[MarketService] Fetching Company Profile for {}...", 
        ticker);

    auto api_profile = co_await provider_manager_.GetCompanyProfile(ticker);

    if (api_profile.has_value()) {
        co_await stock_repo_->SaveCompanyAsync(api_profile.value());

        Logger::Debug(
            "[MarketService] Fetched and saved Company Profile for {}", 
            ticker);
        
        co_return co_await stock_repo_->GetCompanyByTickerAsync(ticker);
    }

    if (db_profile.has_value()) {
        Logger::Warn(
            "[MarketService] API failed, returning stale Company Profile for {}", 
            ticker);
        co_return db_profile;
    }

    Logger::Warn(
        "[MarketService] Failed to get Company Profile for {}", 
        ticker);
    co_return std::nullopt;
}

boost::asio::awaitable<std::vector<StockPriceCandle>> MarketService::GetStockHistory(
        const std::string& ticker, 
        Timestamp from, 
        Timestamp to, 
        TimeFrame interval) {
    Logger::Debug(
        "[MarketService] Requesting Stock History for {}", 
        ticker);
    
    auto history = co_await provider_manager_.GetStockHistory(ticker, from, to, interval);
    
    if (history.empty()) {
        Logger::Warn(
            "[MarketService] Provider returned empty Stock History for {}",
            ticker);
        co_return std::vector<StockPriceCandle>{};
    }

    auto company_id_opt = co_await EnsureCompanyExists(ticker);
    if (company_id_opt.has_value()) {
        co_await stock_repo_->SaveStockPricesBatchAsync(company_id_opt.value(), history);
        Logger::Debug(
            "[MarketService] Saved {} Stock History for {}", 
            history.size(), ticker);
    } else {
        Logger::Warn(
            "[MarketService] Cannot save Stock History: company {} not found", 
            ticker);
    }

    co_return history;
}

boost::asio::awaitable<std::vector<StockDividends>> MarketService::GetDividends(
        const std::string& ticker,
        Date from, 
        Date to) {
    const auto TIME_LIMIT = std::chrono::hours(24);
    
    auto company_opt = co_await stock_repo_->GetCompanyByTickerAsync(ticker);
    bool need_fetch = true;

    if (company_opt.has_value()) {
        auto last_update_opt = co_await stock_repo_->GetLastUpdateAsync(company_opt->id, "dividends");
        
        if (last_update_opt.has_value()) {
            auto age = std::chrono::system_clock::now() - last_update_opt.value();

            if (age < TIME_LIMIT) {
                Logger::Debug(
                    "[MarketService] Dividends for {} updated < 24h ago", 
                    ticker);
                need_fetch = false;
            } else {
                Logger::Debug(
                    "[MarketService] Dividends for {} are stale. Refreshing...", 
                    ticker);
            }
        } else {
            Logger::Debug(
                "[MarketService] No Dividends log for {}. Fetching...",
                ticker);
        }

        if (!need_fetch) {
            auto db_divs = co_await stock_repo_->GetDividendsAsync(company_opt->id, from, to);

            if (!db_divs.empty()) {
                co_return db_divs;
            }
        }
    }

    auto api_divs = co_await provider_manager_.GetDividends(ticker, from, to);
    
    if (!api_divs.empty()) {
        auto company_id_opt = co_await EnsureCompanyExists(ticker);

        if (company_id_opt.has_value()) {
            co_await stock_repo_->SaveStockDividendsBatchAsync(company_id_opt.value(), api_divs);
            co_await stock_repo_->SetLastUpdateAsync(company_id_opt.value(), "dividends");
            
            Logger::Debug(
                "[MarketService] Updated Dividends for {}", 
                ticker);
        }
        co_return api_divs;
    }

    if (company_opt.has_value()) {
        Logger::Warn(
            R"([MarketService] API returned no Dividends, returning cached mb older than {}h "
            "Dividends for {})", 
            TIME_LIMIT.count(), ticker);
        co_return co_await stock_repo_->GetDividendsAsync(company_opt->id, from, to);
    }
    co_return std::vector<StockDividends>{};
}

boost::asio::awaitable<std::vector<CompanyFinancialReport>> MarketService::GetFinancialReports(
        const std::string& ticker) {
    const auto TIME_LIMIT = std::chrono::hours(24 * 3);

    auto company_opt = co_await stock_repo_->GetCompanyByTickerAsync(ticker);
    bool need_fetch = true;

    if (company_opt.has_value()) {
        auto last_update_opt = co_await stock_repo_->GetLastUpdateAsync(company_opt->id, "Financials Reports");

        if (last_update_opt.has_value()) {
            auto age = std::chrono::system_clock::now() - last_update_opt.value();

            if (age < TIME_LIMIT) {
                Logger::Debug(
                    "[MarketService] Financials Reports for {} are fresh.", 
                    ticker);
                need_fetch = false;
            } else {
                Logger::Debug(
                    "[MarketService] Financials Reports for {} are stale. Refreshing...", 
                    ticker);
            }
        }
        
        if (!need_fetch) {
            auto db_reports = co_await stock_repo_->GetFinancialReportsAsync(company_opt->id);

            if (!db_reports.empty()) {
                co_return db_reports;
            }
        }
    }

    auto api_reports = co_await provider_manager_.GetFinancialReports(ticker);
    
    if (!api_reports.empty()) {
        auto company_id_opt = co_await EnsureCompanyExists(ticker);
        if (company_id_opt.has_value()) {
            co_await stock_repo_->SaveCompanyFinancialReportsBatchAsync(company_id_opt.value(), api_reports);
            co_await stock_repo_->SetLastUpdateAsync(company_id_opt.value(), "Financials Reports");

            Logger::Debug(
                "[MarketService] Updated Financials Reports for {}", 
                ticker);
        }
        co_return api_reports;
    }

    if (company_opt.has_value()) {
        Logger::Warn(
            R"([MarketService] API returned no Financial Reports, returning cached mb older "
            "than {}h Financial Reports for {})", 
            TIME_LIMIT.count(), ticker);
        co_return co_await stock_repo_->GetFinancialReportsAsync(company_opt->id);
    }
    co_return std::vector<CompanyFinancialReport>{};
}

boost::asio::awaitable<std::vector<TickerSearchResult>> MarketService::SearchTicker(
        const std::string& query) {
    co_return co_await provider_manager_.SearchStockTicker(query);
}

boost::asio::awaitable<std::optional<double>> MarketService::GetCryptoPrice(const std::string& ticker) {
    const int MAX_TIME_DIFF_MINUTES = 2; 

    auto asset_id_opt = co_await crypto_repo_->GetCryptoAssetIdAsync(ticker);
    
    if (asset_id_opt.has_value()) {
        auto last_candle = co_await crypto_repo_->GetLastCryptoPriceAsync(asset_id_opt.value());

        if (last_candle.has_value()) {
            auto now = std::chrono::system_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::minutes>(now - last_candle->timestamp);
            
            if (diff.count() < MAX_TIME_DIFF_MINUTES) {
                Logger::Debug(
                    "[MarketService] Returning cached Crypto Price for {}", 
                    ticker);
                co_return last_candle->close; 
            }
        }
    }

    Logger::Debug(
        "[MarketService] Fetching fresh Crypto Price for {}...", 
        ticker);

    auto price_opt = co_await provider_manager_.GetCryptoPrice(ticker);
    
    if (price_opt.has_value()) {
        auto asset_id = co_await EnsureCryptoAssetExists(ticker);

        if (asset_id.has_value()) {
            CryptoPriceCandle candle;
            candle.timestamp = std::chrono::system_clock::now();
            candle.close = price_opt.value();
            candle.open = price_opt.value();
            candle.high = price_opt.value();
            candle.low = price_opt.value();
            candle.volume = 0.0;

            co_await crypto_repo_->SaveCryptoPriceAsync(asset_id.value(), candle);
            Logger::Debug(
                "[MarketService] Saved new Crypto Price for {} to DB", 
                ticker);
        }
        co_return price_opt;
    }

    Logger::Warn(
        "[MarketService] Failed to get Crypto Price for {}", 
        ticker);
    co_return std::nullopt;
}

boost::asio::awaitable<std::optional<CryptoAsset>> MarketService::GetCryptoAssetInfo(
        const std::string& ticker) {
    const auto TTL = std::chrono::hours(24 * 7);

    auto db_asset = co_await crypto_repo_->GetCryptoAssetByTickerAsync(ticker);
    bool need_fetch = true;

    if (db_asset.has_value() && !db_asset->name.empty()) {
        auto age = std::chrono::system_clock::now() - db_asset->updated_at;
        
        if (age < TTL) {
            Logger::Debug(
                "[MarketService] Returning cached Crypto Info for {}", 
                ticker);
            need_fetch = false;

            co_return db_asset;
        } else {
            Logger::Debug(
                "[MarketService] Crypto Info for {} is stale. Refreshing...", 
                ticker);
        }
    }

    Logger::Debug(
        "[MarketService] Fetching Crypto Asset Info for {}...", 
        ticker);

    auto api_asset = co_await provider_manager_.GetCryptoAssetInfo(ticker);

    if (api_asset.has_value()) {
        auto asset_to_save = api_asset.value();
        asset_to_save.ticker = ticker; 

        co_await crypto_repo_->SaveCryptoAssetAsync(asset_to_save);
        Logger::Debug(
            "[MarketService] Saved Crypto Asset Info for {}", 
            ticker);
        co_return co_await crypto_repo_->GetCryptoAssetByTickerAsync(ticker);
    }

    if (db_asset.has_value()) {
            Logger::Warn(
                "[MarketService] API failed, returning stale Crypto Info for {}", 
                ticker);
            co_return db_asset;
    }

    co_return std::nullopt;
}

boost::asio::awaitable<std::vector<CryptoPriceCandle>> MarketService::GetCryptoHistory(
        const std::string& ticker, 
        Timestamp from, 
        Timestamp to, 
        TimeFrame interval) {
    Logger::Debug(
        "[MarketService] Requesting Crypto History for {}", 
        ticker);
    
    auto history = co_await provider_manager_.GetCryptoHistory(ticker, from, to, interval);

    if (history.empty()) {
        Logger::Warn(
            "[MarketService] Provider returned empty Crypto History for {}", 
            ticker);
        co_return std::vector<CryptoPriceCandle>{};
    }

    auto asset_id_opt = co_await EnsureCryptoAssetExists(ticker);
    
    if (asset_id_opt.has_value()) {
        co_await crypto_repo_->SaveCryptoPricesBatchAsync(asset_id_opt.value(), history);
        Logger::Debug(
            "[MarketService] Saved {} Crypto History candles for {}", 
            history.size(), ticker);
    } else {
        Logger::Warn(
            "[MarketService] Failed to save crypto history: could not create asset {}", 
            ticker);
    }

    co_return history;
}

boost::asio::awaitable<std::vector<CryptoAsset>> MarketService::GetTopCryptoAssets(int limit) {
    co_return co_await provider_manager_.GetCryptoTopList(limit);
}

boost::asio::awaitable<std::optional<int>> MarketService::EnsureCompanyExists(const std::string& ticker) {
    auto id_opt = co_await stock_repo_->GetCompanyIdAsync(ticker);

    if (id_opt.has_value()) {
        co_return id_opt;
    }

    Logger::Debug(
        "[MarketService] Auto-provisioning Company Profile {}...", 
        ticker);
    
    auto profile_opt = co_await GetCompanyProfile(ticker);

    if (profile_opt.has_value()) {
        co_return profile_opt->id;
    }

    Logger::Warn(
        "[MarketService] Could not fetch Company Profile for {}.", 
        ticker);

    CompanyFullInfo c;
    c.ticker = ticker;
    c.name = ticker; 
    c.currency = "USD";
    c.exchange = "UNKNOWN";

    co_await stock_repo_->SaveCompanyAsync(c);

    co_return co_await stock_repo_->GetCompanyIdAsync(c.ticker);
}

boost::asio::awaitable<std::optional<int>> MarketService::EnsureCryptoAssetExists(
        const std::string& ticker) {
    auto id_opt = co_await crypto_repo_->GetCryptoAssetIdAsync(ticker);

    if (id_opt.has_value()) {
        co_return id_opt;
    }

    Logger::Debug(
        "[MarketService] Auto-provisioning Crypto Asset {}...",
        ticker);
    
    auto info_opt = co_await GetCryptoAssetInfo(ticker); 

    if (info_opt.has_value()) {
        co_return info_opt->id;
    }

    Logger::Warn(
        "[MarketService] Could not fetch Crypto Asset for {}.", 
        ticker);

    CryptoAsset a;
    a.ticker = ticker;
    a.name = ticker;

    co_await crypto_repo_->SaveCryptoAssetAsync(a);

    co_return co_await crypto_repo_->GetCryptoAssetIdAsync(ticker);
}