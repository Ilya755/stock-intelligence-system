#pragma once

#include <string>
#include <vector>
#include <map>

#include "boost/asio/awaitable.hpp"

#include "../domain/Entities.hpp"
#include "../common/TimeUtils.hpp"
#include "ProviderCapabilities.hpp"

class IStockProvider {
public:
    virtual ~IStockProvider() = default;

    virtual std::string GetName() const = 0;
    virtual bool HasCapability(const ProviderCapability cap) const = 0;
    virtual boost::asio::awaitable<int> GetRemainingRequests() const = 0; 

    virtual boost::asio::awaitable<std::vector<TickerSearchResult>> SearchStockTicker(
        const std::string& ticker) = 0;

    virtual boost::asio::awaitable<std::optional<double>> GetStockPrice(
        const std::string& ticker) = 0;

    virtual boost::asio::awaitable<std::vector<StockSplit>> GetStockSplits(
        const std::string& ticker, 
        const Date from, 
        const Date to) = 0;

    virtual boost::asio::awaitable<std::vector<StockPriceCandle>> GetStockHistory(
        const std::string& ticker, 
        const Timestamp from, 
        const Timestamp to, 
        const TimeFrame interval) = 0;

    virtual boost::asio::awaitable<std::optional<CompanyFullInfo>> GetCompanyProfile(
        const std::string& ticker) = 0;

    virtual boost::asio::awaitable<std::vector<CompanyFinancialReport>> GetFinancialReports(
        const std::string& ticker) = 0;

    virtual boost::asio::awaitable<std::vector<StockDividends>> GetDividends(
        const std::string& ticker,
        const Date from,
        const Date to) = 0;

    virtual boost::asio::awaitable<std::vector<InsiderTransaction>> GetInsiderTransactions(
        const std::string& ticker, 
        const int limit = 10) = 0;

    virtual boost::asio::awaitable<std::optional<AnalystRating>> GetAnalystRatings(
        const std::string& ticker) = 0;
    
    virtual boost::asio::awaitable<std::map<std::string, double>> GetTechnicalIndicator(
        const std::string& ticker, 
        const TechIndicatorType type, 
        const TimeFrame interval) = 0;

    virtual boost::asio::awaitable<std::vector<MarketNews>> GetCompanyNews(
        const std::string& ticker, 
        const int limit = 5) = 0;

    virtual boost::asio::awaitable<std::vector<MarketNews>> GetMarketNews(
        const std::string& category = "general", 
        const int limit = 10) = 0;
    

    virtual boost::asio::awaitable<std::vector<CalendarEvent>> GetEarningsCalendar(
        const Date from, 
        const Date to) = 0;

    virtual boost::asio::awaitable<std::vector<EconomicIndicator>> GetMacroIndicator(
        const MacroIndicatorType type) = 0;
};