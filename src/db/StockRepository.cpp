#include "StockRepository.hpp"

#include <format>
#include <stdexcept>
#include <string>
#include <utility>

#include "nlohmann/json.hpp"

#include "../common/TimeUtils.hpp"
#include "../common/Logger.hpp"

namespace {

std::string Text(const PgResult& result, int row, int column) {
    return std::string(result.Value(row, column));
}

PgParam Param(int value) {
    return std::to_string(value);
}

PgParam Param(long long value) {
    return std::to_string(value);
}

PgParam Param(double value) {
    return std::format("{}", value);
}

PgParam Param(std::string value) {
    return value;
}

nlohmann::json OptionalText(const std::optional<std::string>& value) {
    return value.has_value() ? nlohmann::json(*value) : nlohmann::json(nullptr);
}

}

StockRepository::StockRepository(Database& db)
    : db_(db)
    {}

boost::asio::awaitable<void> StockRepository::SaveCompanyAsync(CompanyFullInfo company) {
    try {
        co_await db_.Query(
            "INSERT INTO companies(ticker, name, country, sector, "
            "industry, currency, description, exchange) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8) "
            "ON CONFLICT (ticker, exchange) DO UPDATE "
            "SET name = EXCLUDED.name, country = EXCLUDED.country, "
            "sector = EXCLUDED.sector, industry = EXCLUDED.industry, "
            "description = EXCLUDED.description, updated_at = NOW()",
            {
                Param(company.ticker),
                Param(company.name),
                Param(company.country.value_or("")),
                Param(company.sector.value_or("")),
                Param(company.industry.value_or("")),
                Param(company.currency),
                Param(company.description.value_or("")),
                Param(company.exchange)
            });

        Logger::Debug(
            "[StockRepository] Company saved: {}", 
            company.ticker);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Failed to save company {}: {}", 
            company.ticker, ex.what());
    }
}

boost::asio::awaitable<void> StockRepository::SaveCompaniesBatchAsync(
        std::vector<CompanyFullInfo> companies) {
    if (companies.empty()) {
        co_return;
    }

    try {
        nlohmann::json rows = nlohmann::json::array();
        for (const auto& company : companies) {
            rows.push_back({
                {"ticker", company.ticker},
                {"name", company.name},
                {"country", OptionalText(company.country)},
                {"sector", OptionalText(company.sector)},
                {"industry", OptionalText(company.industry)},
                {"currency", company.currency},
                {"description", OptionalText(company.description)},
                {"exchange", company.exchange},
                {"updated_at", TimeUtils::TimestampToString(company.updated_at)}
            });
        }

        co_await db_.Query(
            "INSERT INTO companies(ticker, name, country, sector, "
            "industry, currency, description, exchange, updated_at) "
            "SELECT ticker, name, country, sector, industry, "
            "currency, description, exchange, updated_at "
            "FROM jsonb_to_recordset($1::jsonb) "
            "AS x(ticker text, name text, country text, sector text, industry text, "
            "currency text, description text, exchange text, updated_at timestamp) "
            "ON CONFLICT (ticker, exchange) DO UPDATE "
            "SET name = EXCLUDED.name, country = EXCLUDED.country, "
            "sector = EXCLUDED.sector, industry = EXCLUDED.industry, "
            "description = EXCLUDED.description, updated_at = NOW()",
            {Param(rows.dump())});

        Logger::Debug(
            "[StockRepository] Batch insert companies completed");
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Batch insert companies failed: {}", 
            ex.what());
    }
}

boost::asio::awaitable<void> StockRepository::SaveCompanyFinancialReportAsync(
        int company_id, 
        CompanyFinancialReport report) {
    try {
        co_await db_.Query(
            "INSERT INTO companies_financial_reports(company_id, period_date, "
            "report_type, currency, revenue, net_income, total_debt, equity) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8) "
            "ON CONFLICT (company_id, period_date, report_type) DO NOTHING",
            {
                Param(company_id),
                Param(TimeUtils::DateToString(report.period_date)),
                Param(ReportTypeToString(report.report_type)),
                Param(report.currency),
                Param(report.revenue),
                Param(report.net_income),
                Param(report.total_debt),
                Param(report.equity)
            });

        Logger::Debug(
            "[StockRepository] Financial report for company_id {} saved", 
            company_id);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Failed to save financial report for company_id {}: {}",
            company_id, ex.what());
    }
}

boost::asio::awaitable<void> StockRepository::SaveCompanyFinancialReportsBatchAsync(
        int company_id, 
        std::vector<CompanyFinancialReport> financial_reports) {
    if (financial_reports.empty()) {
        co_return;
    }

    try {
        nlohmann::json rows = nlohmann::json::array();
        for (const auto& report : financial_reports) {
            rows.push_back({
                {"company_id", company_id},
                {"period_date", TimeUtils::DateToString(report.period_date)},
                {"report_type", ReportTypeToString(report.report_type)},
                {"currency", report.currency},
                {"revenue", report.revenue},
                {"net_income", report.net_income},
                {"total_debt", report.total_debt},
                {"equity", report.equity}
            });
        }

        co_await db_.Query(
            "INSERT INTO companies_financial_reports(company_id, period_date, "
            "report_type, currency, revenue, net_income, total_debt, equity) "
            "SELECT company_id, period_date, report_type, currency, revenue, "
            "net_income, total_debt, equity "
            "FROM jsonb_to_recordset($1::jsonb) "
            "AS x(company_id int, period_date date, report_type report_type_enum, "
            "currency text, revenue numeric, net_income numeric, total_debt numeric, "
            "equity numeric) "
            "ON CONFLICT (company_id, period_date, report_type) DO NOTHING",
            {Param(rows.dump())});

        Logger::Debug(
            "[StockRepository] Batch insert company {} financial reports completed", 
            company_id);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Batch insert company {} financial reports failed", 
            company_id);
    }
}

boost::asio::awaitable<void> StockRepository::SaveStockDividendsAsync(
        int company_id, 
        StockDividends dividends) {
    PgParam payment_date = std::nullopt;
    if (dividends.payment_date.has_value()) {
        payment_date = TimeUtils::DateToString(*dividends.payment_date);
    }

    try {
        co_await db_.Query(
            "INSERT INTO stock_dividends(company_id, ex_date, payment_date, amount) "
            "VALUES ($1, $2, $3, $4) "
            "ON CONFLICT (company_id, ex_date) DO NOTHING",
            {
                Param(company_id),
                Param(TimeUtils::DateToString(dividends.ex_date)),
                std::move(payment_date),
                Param(dividends.amount)
            });

        Logger::Debug(
            "[StockRepository] Stock dividends for company_id {} saved", 
            company_id);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Failed to save stock dividends for company_id {}: {}", 
            company_id, ex.what());
    }
}

boost::asio::awaitable<void> StockRepository::SaveStockDividendsBatchAsync(
        int company_id, 
        std::vector<StockDividends> dividends) {
    if (dividends.empty()) {
        co_return;
    }

    try {
        nlohmann::json rows = nlohmann::json::array();
        for (const auto& dividend : dividends) {
            nlohmann::json payment_date = nullptr;
            if (dividend.payment_date.has_value()) {
                payment_date = TimeUtils::DateToString(*dividend.payment_date);
            }
            rows.push_back({
                {"company_id", company_id},
                {"ex_date", TimeUtils::DateToString(dividend.ex_date)},
                {"payment_date", std::move(payment_date)},
                {"amount", dividend.amount}
            });
        }

        co_await db_.Query(
            "INSERT INTO stock_dividends(company_id, ex_date, payment_date, amount) "
            "SELECT company_id, ex_date, payment_date, amount "
            "FROM jsonb_to_recordset($1::jsonb) "
            "AS x(company_id int, ex_date date, payment_date date, amount numeric) "
            "ON CONFLICT (company_id, ex_date) DO UPDATE "
            "SET payment_date = EXCLUDED.payment_date, amount = EXCLUDED.amount",
            {Param(rows.dump())});

        Logger::Debug(
            "[StockRepository] Batch insert company {} stock dividends completed", 
            company_id);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Batch insert company {} dividends failed: {}", 
            company_id, ex.what());
    }
}

boost::asio::awaitable<void> StockRepository::SaveStockPriceAsync(
        int company_id, 
        StockPriceCandle stock_price) {
    try {
        co_await db_.Query(
            "INSERT INTO stock_prices(company_id, timestamp, open, "
            "high, low, close, volume) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7) "
            "ON CONFLICT (company_id, timestamp) DO NOTHING",
            {
                Param(company_id),
                Param(TimeUtils::TimestampToString(stock_price.timestamp)),
                Param(stock_price.open),
                Param(stock_price.high),
                Param(stock_price.low),
                Param(stock_price.close),
                Param(stock_price.volume)
            });

        Logger::Debug(
            "[StockRepository] Stock price for company_id {} saved", 
            company_id);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Failed to save stock price for company_id {}: {}",
            company_id, ex.what());
    }
}

boost::asio::awaitable<void> StockRepository::SaveStockPricesBatchAsync(
        int company_id, 
        std::vector<StockPriceCandle> prices) {
    if (prices.empty()) {
        co_return;
    }
    
    try {
        nlohmann::json rows = nlohmann::json::array();
        for (const auto& candle : prices) {
            rows.push_back({
                {"company_id", company_id},
                {"timestamp", TimeUtils::TimestampToString(candle.timestamp)},
                {"open", candle.open},
                {"high", candle.high},
                {"low", candle.low},
                {"close", candle.close},
                {"volume", candle.volume}
            });
        }

        co_await db_.Query(
            "INSERT INTO stock_prices(company_id, timestamp, open, "
            "high, low, close, volume) "
            "SELECT company_id, timestamp, open, high, low, close, volume "
            "FROM jsonb_to_recordset($1::jsonb) "
            "AS x(company_id int, timestamp timestamp, open numeric, high numeric, "
            "low numeric, close numeric, volume bigint) "
            "ON CONFLICT (company_id, timestamp) DO NOTHING",
            {Param(rows.dump())});

        Logger::Debug(
            "[StockRepository] Batch insert prices for company {} completed", 
            company_id);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Batch insert prices failed for company {}: {}", 
            company_id, ex.what());
    }
}

boost::asio::awaitable<void> StockRepository::SaveStockSplitAsync(
        int company_id, 
        StockSplit split) {
    try {
        co_await db_.Query(
            "INSERT INTO stock_splits(company_id, split_date, numerator, denominator) "
            "VALUES ($1, $2, $3, $4) "
            "ON CONFLICT (company_id, split_date) DO UPDATE "
            "SET numerator = EXCLUDED.numerator, denominator = EXCLUDED.denominator",
            {
                Param(company_id),
                Param(TimeUtils::DateToString(split.date)),
                Param(split.numerator),
                Param(split.denominator)
            });

        Logger::Debug(
            "[StockRepository] Stock split saved for company_id: {}", 
            company_id);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Failed to save stock split for company_id {}: {}", 
            company_id, ex.what());
    }
}

boost::asio::awaitable<void> StockRepository::SaveStockSplitsBatchAsync(
        int company_id, 
        std::vector<StockSplit> splits) {
    if (splits.empty()) {
        co_return;
    }

    try {
        nlohmann::json rows = nlohmann::json::array();
        for (const auto& split : splits) {
            rows.push_back({
                {"company_id", company_id},
                {"split_date", TimeUtils::DateToString(split.date)},
                {"numerator", split.numerator},
                {"denominator", split.denominator}
            });
        }

        co_await db_.Query(
            "INSERT INTO stock_splits(company_id, split_date, numerator, denominator) "
            "SELECT company_id, split_date, numerator, denominator "
            "FROM jsonb_to_recordset($1::jsonb) "
            "AS x(company_id int, split_date date, numerator numeric, denominator numeric) "
            "ON CONFLICT (company_id, split_date) DO UPDATE "
            "SET numerator = EXCLUDED.numerator, denominator = EXCLUDED.denominator",
            {Param(rows.dump())});

        Logger::Debug(
            "[StockRepository] Batch insert stock splits completed for company_id: {}", 
            company_id);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Batch insert stock splits failed for company_id {}: {}", 
            company_id, ex.what());
    }
}

boost::asio::awaitable<std::vector<StockPriceCandle>> StockRepository::GetHistoryStockPricesAsync(
        int company_id, 
        Timestamp from, 
        Timestamp to) {
    std::vector<StockPriceCandle> prices;
    try {
        auto result = co_await db_.Query(
            "SELECT timestamp, open, high, low, close, volume "
            "FROM stock_prices "
            "WHERE company_id = $1 AND timestamp >= $2 AND timestamp <= $3 "
            "ORDER BY timestamp ASC",
            {
                Param(company_id),
                Param(TimeUtils::TimestampToString(from)),
                Param(TimeUtils::TimestampToString(to))
            });

        prices.reserve(static_cast<std::size_t>(result.RowCount()));

        for (int row = 0; row < result.RowCount(); ++row) {
            prices.push_back({
                TimeUtils::StringToTimestamp(Text(result, row, 0)),
                std::stod(Text(result, row, 1)),
                std::stod(Text(result, row, 2)),
                std::stod(Text(result, row, 3)),
                std::stod(Text(result, row, 4)),
                std::stoll(Text(result, row, 5))
            });
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] GetHistoryStockCandles failed: {}", 
            ex.what());
    }
    co_return prices;
}

boost::asio::awaitable<std::optional<int>> StockRepository::GetCompanyIdAsync(
        std::string ticker) {
    try {
        auto result = co_await db_.Query(
            "SELECT id "
            "FROM companies "
            "WHERE ticker = $1",
            {Param(std::move(ticker))});
        if (result.RowCount() == 0) {
            co_return std::nullopt;
        }

        co_return std::stoi(Text(result, 0, 0));
    } catch (const std::exception& ex) {
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::optional<CompanyFullInfo>> StockRepository::GetCompanyByTickerAsync(
        std::string ticker) {
    try {
        auto result = co_await db_.Query(
            "SELECT id, ticker, name, country, sector, industry, "
            "currency, description, exchange, updated_at "
            "FROM companies "
            "WHERE ticker = $1",
            {Param(std::move(ticker))});
        if (result.RowCount() == 0) {
            co_return std::nullopt;
        }

        CompanyFullInfo company;
        company.id = std::stoi(Text(result, 0, 0));
        company.ticker = Text(result, 0, 1);
        company.name = Text(result, 0, 2);
        if (!result.IsNull(0, 3)) {
            company.country = Text(result, 0, 3);
        }
        if (!result.IsNull(0, 4)) {
            company.sector = Text(result, 0, 4);
        }
        if (!result.IsNull(0, 5)) {
            company.industry = Text(result, 0, 5);
        }
        company.currency = Text(result, 0, 6);
        if (!result.IsNull(0, 7)) {
            company.description = Text(result, 0, 7);
        }
        company.exchange = Text(result, 0, 8);
        company.updated_at = TimeUtils::StringToTimestamp(Text(result, 0, 9));

        co_return company;
    } catch (const std::exception& ex) {
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::vector<CompanyPreview>> StockRepository::GetAllCompaniesPreviewAsync() {
    std::vector<CompanyPreview> companies;
    try {
        auto result = co_await db_.Query(
            "SELECT id, ticker, name, country, sector, currency "
            "FROM companies "
            "ORDER BY ticker");

        companies.reserve(static_cast<std::size_t>(result.RowCount()));

        for (int row = 0; row < result.RowCount(); ++row) {
            CompanyPreview company;
            company.id = std::stoi(Text(result, row, 0));
            company.ticker = Text(result, row, 1);
            company.name = Text(result, row, 2);
            if (!result.IsNull(row, 3)) {
                company.country = Text(result, row, 3);
            }
            if (!result.IsNull(row, 4)) {
                company.sector = Text(result, row, 4);
            }
            company.currency = Text(result, row, 5);
            companies.push_back(std::move(company));
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Failed to get all companies: {}", 
            ex.what());
    }
    co_return companies;
}

boost::asio::awaitable<std::optional<StockPriceCandle>> StockRepository::GetLastStockPriceAsync(
        int company_id) {
    try {
        auto result = co_await db_.Query(
            "SELECT timestamp, open, high, low, close, volume "
            "FROM stock_prices "
            "WHERE company_id = $1 "
            "ORDER BY timestamp DESC "
            "LIMIT 1",
            {Param(company_id)});
        if (result.RowCount() == 0) {
            co_return std::nullopt;
        }

        co_return StockPriceCandle{
            TimeUtils::StringToTimestamp(Text(result, 0, 0)),
            std::stod(Text(result, 0, 1)),
            std::stod(Text(result, 0, 2)),
            std::stod(Text(result, 0, 3)),
            std::stod(Text(result, 0, 4)),
            std::stoll(Text(result, 0, 5))
        };
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Failed to get last stock price for {}: {}", 
            company_id, ex.what());
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::vector<StockDividends>> StockRepository::GetDividendsAsync(
        int company_id, 
        Date from, 
        Date to) {
    std::vector<StockDividends> dividends;
    try {
        auto result = co_await db_.Query(
            "SELECT ex_date, payment_date, amount "
            "FROM stock_dividends "
            "WHERE company_id = $1 AND ex_date >= $2 AND ex_date <= $3 "
            "ORDER BY ex_date DESC",
            {
                Param(company_id),
                Param(TimeUtils::DateToString(from)),
                Param(TimeUtils::DateToString(to))
            });

        dividends.reserve(static_cast<std::size_t>(result.RowCount()));

        for (int row = 0; row < result.RowCount(); ++row) {
            StockDividends dividend;
            dividend.ex_date = TimeUtils::StringToDate(Text(result, row, 0));
            if (!result.IsNull(row, 1)) {
                dividend.payment_date = TimeUtils::StringToDate(Text(result, row, 1));
            }
            dividend.amount = std::stod(Text(result, row, 2));
            dividends.push_back(std::move(dividend));
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Failed to get dividends: {}", 
            ex.what());
    }
    co_return dividends;
}

boost::asio::awaitable<std::vector<CompanyFinancialReport>> StockRepository::GetFinancialReportsAsync(
        int company_id) {
    std::vector<CompanyFinancialReport> reports;
    try {
        auto result = co_await db_.Query(
            "SELECT period_date, report_type, currency, revenue, "
            "net_income, total_debt, equity "
            "FROM companies_financial_reports "
            "WHERE company_id = $1 "
            "ORDER BY period_date DESC",
            {Param(company_id)});

        reports.reserve(static_cast<std::size_t>(result.RowCount()));

        for (int row = 0; row < result.RowCount(); ++row) {
            reports.push_back({
                TimeUtils::StringToDate(Text(result, row, 0)),
                StringToReportType(Text(result, row, 1)),
                Text(result, row, 2),
                std::stod(Text(result, row, 3)),
                std::stod(Text(result, row, 4)),
                std::stod(Text(result, row, 5)),
                std::stod(Text(result, row, 6))
            });
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Failed to get financial reports: {}", 
            ex.what());
    }
    co_return reports;
}

boost::asio::awaitable<void> StockRepository::UpdateCompanyDescriptionAsync(
        int company_id, 
        std::string description) {
    try {
        co_await db_.Query(
            "UPDATE companies "
            "SET description = $1, updated_at = NOW() "
            "WHERE id = $2",
            {Param(std::move(description)), Param(company_id)});

        Logger::Debug(
            "[StockRepository] Updated description for company_id: {}", 
            company_id);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Failed to update company {}: {}", 
            company_id, ex.what());
    }
}

boost::asio::awaitable<void> StockRepository::DeleteCompanyAsync(int company_id) {
    try {
        co_await db_.Query(
            "DELETE FROM companies "
            "WHERE id = $1", 
            {Param(company_id)});

        Logger::Info(
            "[StockRepository] Deleted company_id: {}", 
            company_id);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Failed to delete company {}: {}", 
            company_id, ex.what());
    }
}

boost::asio::awaitable<void> StockRepository::DeleteOldPricesAsync(
        int company_id, 
        Timestamp older_than) {
    try {
        const auto timestamp = TimeUtils::TimestampToString(older_than);
        co_await db_.Query(
            "DELETE FROM stock_prices "
            "WHERE company_id = $1 AND timestamp < $2",
            {Param(company_id), Param(timestamp)});

        Logger::Info(
            "[StockRepository] Deleted company {} prices older than {}", 
            company_id, TimeUtils::TimestampToString(older_than));
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Failed to cleanup prices for {} older than {}: {}", 
            company_id, TimeUtils::TimestampToString(older_than), ex.what());
    }
}

boost::asio::awaitable<std::optional<Timestamp>> StockRepository::GetLastUpdateAsync(
        int company_id, 
        std::string data_type) {
    try {
        auto result = co_await db_.Query(
            "SELECT updated_at "
            "FROM data_update_logs "
            "WHERE company_id = $1 AND data_type = $2",
            {Param(company_id), Param(std::move(data_type))});
        if (result.RowCount() == 0) {
            co_return std::nullopt;
        }

        co_return TimeUtils::StringToTimestamp(Text(result, 0, 0));
    } catch (const std::exception&) {
        co_return std::nullopt;
    }
}

boost::asio::awaitable<void> StockRepository::SetLastUpdateAsync(
        int company_id, 
        std::string data_type) {
    try {
        co_await db_.Query(
            "INSERT INTO data_update_logs(company_id, data_type, updated_at) "
            "VALUES ($1, $2, NOW()) "
            "ON CONFLICT (company_id, data_type) DO UPDATE "
            "SET updated_at = NOW()",
            {Param(company_id), Param(std::move(data_type))});
    } catch (const std::exception& ex) {
        Logger::Error(
            "[StockRepository] Failed to set last update log: {}", 
            ex.what());
    }
}

std::string StockRepository::ReportTypeToString(ReportType type) {
    switch (type) {
        case ReportType::Annual: 
            return "annual";
        case ReportType::Quarterly:
            return "quarterly";
        case ReportType::Ttm:
            return "ttm";
    }

    Logger::Error(
        "[StockRepository] Couldn't convert ReportType type to string");
    throw std::runtime_error("Couldn't convert ReportType type to string");
}

ReportType StockRepository::StringToReportType(const std::string& value) {
    if (value == "annual") {
        return ReportType::Annual;
    } else if (value == "quarterly") {
        return ReportType::Quarterly;
    } else if (value == "ttm") {
        return ReportType::Ttm;
    }
    
    Logger::Error(
        "[StockRepository] Couldn't convert string type to ReportType: {}", 
        value);
    throw std::runtime_error("Couldn't convert string '" + value + "' to ReportType");
}