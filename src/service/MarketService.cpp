#include "MarketService.hpp"

#include "../common/Logger.hpp"

MarketService::MarketService(std::shared_ptr<StockRepository> stock_repo,
                                std::shared_ptr<CryptoRepository> crypto_repo)
    : stock_repo_(stock_repo) 
    , crypto_repo_(crypto_repo)
    , provider_manager_(ProviderManager::GetInstance()) 
    {}

std::optional<double> MarketService::GetStockPrice(const std::string& ticker) {
    const int MAX_TIME_DIFF_MINUTES = 5;

    auto company_opt = stock_repo_->GetCompanyByTicker(ticker);
    
    if (company_opt.has_value()) {
        auto last_candle = stock_repo_->GetLastStockPrice(company_opt->id);

        if (last_candle.has_value()) {
            auto now = std::chrono::system_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::minutes>(now - last_candle->timestamp);
            
            if (diff.count() < MAX_TIME_DIFF_MINUTES) {
                Logger::Debug("[MarketService] Returning cached stock price for {}", ticker);
                return last_candle->close; 
            }
        }
    }

    Logger::Debug("[MarketService] Fetching fresh stock price for {} from providers...", ticker);

    auto price_opt = provider_manager_.GetStockPrice(ticker);
    
    if (price_opt.has_value()) {
        auto company_id_opt = EnsureCompanyExists(ticker);

        if (company_id_opt.has_value()) {
            StockPriceCandle candle;
            candle.timestamp = std::chrono::system_clock::now();
            candle.close = price_opt.value();
            candle.open = price_opt.value();
            candle.high = price_opt.value();
            candle.low = price_opt.value();
            candle.volume = 0; 

            stock_repo_->SaveStockPrice(company_id_opt.value(), candle);
            Logger::Debug("[MarketService] Saved new Stock Price for {} to DB", ticker);
        }
        return price_opt;
    }

    Logger::Warn("[MarketService] Failed to get Stock Price for {}", ticker);
    return std::nullopt;
}

std::optional<CompanyFullInfo> MarketService::GetCompanyProfile(const std::string& ticker) {
    const auto TIME_LIMIT = std::chrono::hours(24 * 7);

    auto db_profile = stock_repo_->GetCompanyByTicker(ticker);
    bool need_fetch = true;

    if (db_profile.has_value()) {
        auto age = std::chrono::system_clock::now() - db_profile->updated_at;

        if (age < TIME_LIMIT) {
            Logger::Debug("[MarketService] Returning cached Company Profile for {}", ticker);
            need_fetch = false;

            return db_profile;
        } else {
            Logger::Debug("[MarketService] Company Profile for {} is stale. Refreshing...", ticker);
        }
    }

    Logger::Debug("[MarketService] Fetching Company Profile for {}...", ticker);

    auto api_profile = provider_manager_.GetCompanyProfile(ticker);

    if (api_profile.has_value()) {
        stock_repo_->SaveCompany(api_profile.value());

        Logger::Debug("[MarketService] Fetched and saved Company Profile for {}", ticker);
        
        return stock_repo_->GetCompanyByTicker(ticker);
    }

    if (db_profile.has_value()) {
        Logger::Warn("[MarketService] API failed, returning stale Company Profile for {}", ticker);
        return db_profile;
    }

    Logger::Warn("[MarketService] Failed to get Company Profile for {}", ticker);
    return std::nullopt;
}

std::vector<StockPriceCandle> MarketService::GetStockHistory(const std::string& ticker, 
                                                                Timestamp from, 
                                                                Timestamp to, 
                                                                TimeFrame interval) {
    Logger::Debug("[MarketService] Requesting Stock History for {}", ticker);
    
    auto history = provider_manager_.GetStockHistory(ticker, from, to, interval);
    
    if (history.empty()) {
        Logger::Warn("[MarketService] Provider returned empty Stock History for {}", ticker);
        return {};
    }

    auto company_id_opt = EnsureCompanyExists(ticker);
    if (company_id_opt.has_value()) {
        stock_repo_->SaveStockPricesBatch(company_id_opt.value(), history);
        Logger::Debug("[MarketService] Saved {} Stock History for {}", history.size(), ticker);
    } else {
        Logger::Warn("[MarketService] Cannot save Stock History: company {} not found", ticker);
    }

    return history;
}

std::vector<StockDividends> MarketService::GetDividends(const std::string& ticker, Date from, Date to) {
    const auto TIME_LIMIT = std::chrono::hours(24);
    
    auto company_opt = stock_repo_->GetCompanyByTicker(ticker);
    bool need_fetch = true;

    if (company_opt.has_value()) {
        auto last_update_opt = stock_repo_->GetLastUpdate(company_opt->id, "dividends");
        
        if (last_update_opt.has_value()) {
            auto age = std::chrono::system_clock::now() - last_update_opt.value();

            if (age < TIME_LIMIT) {
                Logger::Debug("[MarketService] Dividends for {} updated < 24h ago", ticker);
                need_fetch = false;
            } else {
                Logger::Debug("[MarketService] Dividends for {} are stale. Refreshing...", ticker);
            }
        } else {
            Logger::Debug("[MarketService] No Dividends log for {}. Fetching...", ticker);
        }

        if (!need_fetch) {
            auto db_divs = stock_repo_->GetDividends(company_opt->id, from, to);

            if (!db_divs.empty()) {
                return db_divs;
            }
        }
    }

    auto api_divs = provider_manager_.GetDividends(ticker, from, to);
    
    if (!api_divs.empty()) {
        auto company_id_opt = EnsureCompanyExists(ticker);

        if (company_id_opt.has_value()) {
            stock_repo_->SaveStockDividendsBatch(company_id_opt.value(), api_divs);
            stock_repo_->SetLastUpdate(company_id_opt.value(), "dividends");
            
            Logger::Debug("[MarketService] Updated Dividends for {}", ticker);
        }
        return api_divs;
    }

    if (company_opt.has_value()) {
        Logger::Warn(R"([MarketService] API returned no Dividends, returning "
                        "cached mb older than {}h Dividends for {})", TIME_LIMIT.count(), ticker);
        return stock_repo_->GetDividends(company_opt->id, from, to);
    }
    return {};
}

std::vector<CompanyFinancialReport> MarketService::GetFinancialReports(const std::string& ticker) {
    const auto TIME_LIMIT = std::chrono::hours(24 * 3);

    auto company_opt = stock_repo_->GetCompanyByTicker(ticker);
    bool need_fetch = true;

    if (company_opt.has_value()) {
        auto last_update_opt = stock_repo_->GetLastUpdate(company_opt->id, "financials");

        if (last_update_opt.has_value()) {
            auto age = std::chrono::system_clock::now() - last_update_opt.value();

            if (age < TIME_LIMIT) {
                Logger::Debug("[MarketService] Financials Reports for {} are fresh.", ticker);
                need_fetch = false;
            } else {
                Logger::Debug("[MarketService] Financials Reports for {} are stale. Refreshing...", ticker);
            }
        }
        
        if (!need_fetch) {
            auto db_reports = stock_repo_->GetFinancialReports(company_opt->id);

            if (!db_reports.empty()) {
                return db_reports;
            }
        }
    }

    auto api_reports = provider_manager_.GetFinancialReports(ticker);
    
    if (!api_reports.empty()) {
        auto company_id_opt = EnsureCompanyExists(ticker);
        if (company_id_opt.has_value()) {
            stock_repo_->SaveCompanyFinancialReportsBatch(company_id_opt.value(), api_reports);
            stock_repo_->SetLastUpdate(company_id_opt.value(), "Financials Reports");

            Logger::Debug("[MarketService] Updated Financials Reports for {}", ticker);
        }
        return api_reports;
    }

    if (company_opt.has_value()) {
        Logger::Warn(R"([MarketService] API returned no Financial Reports, returning "
                        "cached mb older than {}h Financial Reports for {})", TIME_LIMIT.count(), ticker);
        return stock_repo_->GetFinancialReports(company_opt->id);
    }
    return {};
}

std::vector<TickerSearchResult> MarketService::SearchTicker(const std::string& query) {
    return provider_manager_.SearchStockTicker(query);
}

std::optional<double> MarketService::GetCryptoPrice(const std::string& ticker) {
    const int MAX_TIME_DIFF_MINUTES = 2; 

    auto asset_id_opt = crypto_repo_->GetCryptoAssetId(ticker);
    
    if (asset_id_opt.has_value()) {
        auto last_candle = crypto_repo_->GetLastCryptoPrice(asset_id_opt.value());

        if (last_candle.has_value()) {
            auto now = std::chrono::system_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::minutes>(now - last_candle->timestamp);
            
            if (diff.count() < MAX_TIME_DIFF_MINUTES) {
                Logger::Debug("[MarketService] Returning cached Crypto Price for {}", ticker);
                return last_candle->close; 
            }
        }
    }

    Logger::Debug("[MarketService] Fetching fresh Crypto Price for {}...", ticker);

    auto price_opt = provider_manager_.GetCryptoPrice(ticker);
    
    if (price_opt.has_value()) {
        auto asset_id = EnsureCryptoAssetExists(ticker);

        if (asset_id.has_value()) {
            CryptoPriceCandle candle;
            candle.timestamp = std::chrono::system_clock::now();
            candle.close = price_opt.value();
            candle.open = price_opt.value();
            candle.high = price_opt.value();
            candle.low = price_opt.value();
            candle.volume = 0.0;

            crypto_repo_->SaveCryptoPrice(asset_id.value(), candle);
            Logger::Debug("[MarketService] Saved new Crypto Price for {} to DB", ticker);
        }
        return price_opt;
    }

    Logger::Warn("[MarketService] Failed to get Crypto Price for {}", ticker);
    return std::nullopt;
}

std::optional<CryptoAsset> MarketService::GetCryptoAssetInfo(const std::string& ticker) {
    const auto TTL = std::chrono::hours(24 * 7);

    auto db_asset = crypto_repo_->GetCryptoAssetByTicker(ticker);
    bool need_fetch = true;

    if (db_asset.has_value() && !db_asset->name.empty()) {
        auto age = std::chrono::system_clock::now() - db_asset->updated_at;
        
        if (age < TTL) {
            Logger::Debug("[MarketService] Returning cached Crypto Info for {}", ticker);
            need_fetch = false;

            return db_asset;
        } else {
            Logger::Debug("[MarketService] Crypto Info for {} is stale. Refreshing...", ticker);
        }
    }

    Logger::Debug("[MarketService] Fetching Crypto Asset Info for {}...", ticker);

    auto api_asset = provider_manager_.GetCryptoAssetInfo(ticker);

    if (api_asset.has_value()) {
        auto asset_to_save = api_asset.value();
        asset_to_save.ticker = ticker; 

        crypto_repo_->SaveCryptoAsset(asset_to_save);
        Logger::Debug("[MarketService] Saved Crypto Asset Info for {}", ticker);
        return crypto_repo_->GetCryptoAssetByTicker(ticker);
    }

    if (db_asset.has_value()) {
            Logger::Warn("[MarketService] API failed, returning stale Crypto Info for {}", ticker);
            return db_asset;
    }

    return std::nullopt;
}

std::vector<CryptoPriceCandle> MarketService::GetCryptoHistory(const std::string& ticker, 
                                                                Timestamp from, 
                                                                Timestamp to, 
                                                                TimeFrame interval) {
    Logger::Debug("[MarketService] Requesting Crypto History for {}", ticker);
    
    auto history = provider_manager_.GetCryptoHistory(ticker, from, to, interval);

    if (history.empty()) {
        Logger::Warn("[MarketService] Provider returned empty Crypto History for {}", ticker);
        return {};
    }

    auto asset_id_opt = EnsureCryptoAssetExists(ticker);
    
    if (asset_id_opt.has_value()) {
        crypto_repo_->SaveCryptoPricesBatch(asset_id_opt.value(), history);
        Logger::Debug("[MarketService] Saved {} Crypto History candles for {}", history.size(), ticker);
    } else {
        Logger::Warn("[MarketService] Failed to save crypto history: could not create asset {}", ticker);
    }

    return history;
}

std::vector<CryptoAsset> MarketService::GetTopCryptoAssets(int limit) {
    return provider_manager_.GetCryptoTopList(limit);
}

std::optional<int> MarketService::EnsureCompanyExists(const std::string& ticker) {
    auto id_opt = stock_repo_->GetCompanyId(ticker);

    if (id_opt.has_value()) {
        return id_opt;
    }

    Logger::Debug("[MarketService] Auto-provisioning Company Profile {}...", ticker);
    
    auto profile_opt = GetCompanyProfile(ticker);

    if (profile_opt.has_value()) {
        return profile_opt->id;
    }

    Logger::Warn("[MarketService] Could not fetch Company Profile for {}.", ticker);
    CompanyFullInfo c;
    c.ticker = ticker;
    c.name = ticker; 
    c.currency = "USD";
    c.exchange = "UNKNOWN";

    stock_repo_->SaveCompany(c);

    return stock_repo_->GetCompanyId(c.ticker);
}

std::optional<int> MarketService::EnsureCryptoAssetExists(const std::string& ticker) {
    auto id_opt = crypto_repo_->GetCryptoAssetId(ticker);

    if (id_opt.has_value()) {
        return id_opt;
    }

    Logger::Debug("[MarketService] Auto-provisioning Crypto Asset {}...", ticker);
    
    auto info_opt = GetCryptoAssetInfo(ticker); 

    if (info_opt.has_value()) {
            return info_opt->id;
    }

    Logger::Warn("[MarketService] Could not fetch Crypto Asset for {}.", ticker);
    CryptoAsset a;
    a.ticker = ticker;
    a.name = ticker;

    crypto_repo_->SaveCryptoAsset(a);

    return crypto_repo_->GetCryptoAssetId(ticker);
}