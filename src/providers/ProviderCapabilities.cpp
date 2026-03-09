#include "ProviderCapabilities.hpp"

std::optional<ProviderCapability> StringToProviderCapability(const std::string& str) {
    static const std::map<std::string, ProviderCapability> mp = {
        {"price_realtime_usa", ProviderCapability::PriceRealtime},
        {"price_intraday", ProviderCapability::PriceIntraday},
        {"price_daily", ProviderCapability::PriceDaily},
        {"price_weekly", ProviderCapability::PriceHistoryDeep},
        {"price_monthly", ProviderCapability::PriceHistoryDeep},

        {"financials_basic", ProviderCapability::FinancialsBasic},
        {"income_statement", ProviderCapability::FinancialsDeep},
        {"balance_sheet", ProviderCapability::FinancialsDeep},
        {"cash_flow_statement", ProviderCapability::FinancialsDeep},
        {"financial_ratios", ProviderCapability::FinancialRatios},

        {"company_profile", ProviderCapability::CompanyProfile},

        {"dividend_calendar", ProviderCapability::Dividends},
        {"dividends_basic", ProviderCapability::Dividends},
        {"dividend_history", ProviderCapability::Dividends},
        {"earnings_calendar", ProviderCapability::Earnings},
        {"earnings_surprises", ProviderCapability::Earnings},
        {"earnings_estimates", ProviderCapability::Earnings},
        {"ipo_calendar", ProviderCapability::Earnings},
        {"stock_splits", ProviderCapability::StockSplits},

        {"technical_indicators", ProviderCapability::TechIndicators},
         {"top_gainers_losers", ProviderCapability::TechIndicators},
        {"analyst_recommendations", ProviderCapability::AnalystRatings},
        {"price_targets", ProviderCapability::AnalystRatings},

        {"news_market", ProviderCapability::News},
        {"news_sentiment", ProviderCapability::Sentiment},

        {"insider_transactions", ProviderCapability::Insiders},
        {"senate_trading", ProviderCapability::Insiders},
        {"institutional_holders", ProviderCapability::Institutional},

        {"macro_gdp", ProviderCapability::MacroEconomics},
        {"macro_inflation", ProviderCapability::MacroEconomics},
        {"macro_fed_rate", ProviderCapability::MacroEconomics},
        {"macro_unemployment", ProviderCapability::MacroEconomics},
        {"global_market_status", ProviderCapability::MacroEconomics},
    };

    auto it = mp.find(str);
    if (it != mp.end()) {
        return it->second;
    }
    
    return std::nullopt;
}