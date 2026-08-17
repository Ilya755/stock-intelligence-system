#pragma once

#include <vector>
#include <string>
#include <optional>

#include "../domain/Entities.hpp"
#include "Database.hpp"

class StockRepository {
public:
    explicit StockRepository(Database& db);

    boost::asio::awaitable<void> SaveCompanyAsync(CompanyFullInfo company);

    boost::asio::awaitable<void> SaveCompaniesBatchAsync(
        std::vector<CompanyFullInfo> companies);

    boost::asio::awaitable<void> SaveCompanyFinancialReportAsync(
        int company_id, 
        CompanyFinancialReport report);

    boost::asio::awaitable<void> SaveCompanyFinancialReportsBatchAsync(
        int company_id, 
        std::vector<CompanyFinancialReport> financial_reports);

    boost::asio::awaitable<void> SaveStockDividendsAsync(
        int company_id, 
        StockDividends dividends);

    boost::asio::awaitable<void> SaveStockDividendsBatchAsync(
        int company_id, 
        std::vector<StockDividends> dividends); 

    boost::asio::awaitable<void> SaveStockPriceAsync(
        int company_id, 
        StockPriceCandle stock_price);

    boost::asio::awaitable<void> SaveStockPricesBatchAsync(
        int company_id, 
        std::vector<StockPriceCandle> prices);

    boost::asio::awaitable<void> SaveStockSplitAsync(int company_id, StockSplit split);

    boost::asio::awaitable<void> SaveStockSplitsBatchAsync(
        int company_id, 
        std::vector<StockSplit> splits);

    boost::asio::awaitable<std::vector<StockPriceCandle>> GetHistoryStockPricesAsync(
        int company_id, 
        Timestamp from, 
        Timestamp to);

    boost::asio::awaitable<std::optional<int>> GetCompanyIdAsync(std::string ticker);

    boost::asio::awaitable<std::optional<CompanyFullInfo>> GetCompanyByTickerAsync(
        std::string ticker); 

    boost::asio::awaitable<std::vector<CompanyPreview>> GetAllCompaniesPreviewAsync(); 

    boost::asio::awaitable<std::optional<StockPriceCandle>> GetLastStockPriceAsync(
        int company_id);

    boost::asio::awaitable<std::vector<StockDividends>> GetDividendsAsync(
        int company_id, 
        Date from, 
        Date to);
        
    boost::asio::awaitable<std::vector<CompanyFinancialReport>> GetFinancialReportsAsync(
        int company_id);
 
    boost::asio::awaitable<std::optional<Timestamp>> GetLastUpdateAsync(
        int company_id, 
        std::string data_type);


    boost::asio::awaitable<void> UpdateCompanyDescriptionAsync(
        int company_id, 
        std::string description);

    boost::asio::awaitable<void> DeleteCompanyAsync(int company_id);

    boost::asio::awaitable<void> DeleteOldPricesAsync(
        int company_id, 
        Timestamp older_than);


    boost::asio::awaitable<void> SetLastUpdateAsync(int company_id, std::string data_type);

private:
    Database& db_;

    static std::string ReportTypeToString(ReportType type);

    static ReportType StringToReportType(const std::string& value);
};