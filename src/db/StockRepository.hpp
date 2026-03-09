#pragma once

#include <vector>
#include <string>
#include <optional>

#include "../domain/Entities.hpp"
#include "Database.hpp"

class StockRepository {
public:
    explicit StockRepository(Database& db);

    void SaveCompany(const CompanyFullInfo& company);

    void SaveCompaniesBatch(const std::vector<CompanyFullInfo>& companies);

    void SaveCompanyFinancialReport(const int company_id, const CompanyFinancialReport& fin_rep);

    void SaveCompanyFinancialReportsBatch(const int company_id,
                                            const std::vector<CompanyFinancialReport>& fin_reps);

    void SaveStockDividends(const int company_id, const StockDividends& dividends);

    void SaveStockDividendsBatch(const int company_id, const std::vector<StockDividends>& dividends); 

    void SaveStockPrice(const int company_id, const StockPriceCandle& stock_price);

    void SaveStockPricesBatch(const int company_id, const std::vector<StockPriceCandle>& prices);

    void SaveStockSplit(const int company_id, const StockSplit& split);

    void SaveStockSplitsBatch(const int company_id, const std::vector<StockSplit>& splits);

    std::vector<StockPriceCandle> GetHistoryStockPrices(const int company_id, 
                                                            const Timestamp from, const Timestamp to);

    std::optional<int> GetCompanyId(const std::string& ticker);

    std::optional<CompanyFullInfo> GetCompanyByTicker(const std::string& ticker); 

    std::vector<CompanyPreview> GetAllCompaniesPreview(); 

    std::optional<StockPriceCandle> GetLastStockPrice(int company_id);

    std::vector<StockDividends> GetDividends(int company_id, const Date from, const Date to);

    std::vector<CompanyFinancialReport> GetFinancialReports(int company_id);

    void UpdateCompanyDescription(int company_id, const std::string& description);

    void DeleteCompany(int company_id);

    void DeleteOldPrices(int company_id, const Timestamp older_than);

    std::optional<Timestamp> GetLastUpdate(int company_id, const std::string& data_type);

    void SetLastUpdate(int company_id, const std::string& data_type);

private:
    Database& db_;

    std::string ReportTypeToString(const ReportType type);

    ReportType StringToReportType(const std::string& str);
};