#include "StockRepository.hpp"

#include "pqxx/pqxx"

#include "../common/TimeUtils.hpp"
#include "../common/Logger.hpp"

StockRepository::StockRepository(Database& db)
    : db_(db)
    {}

void StockRepository::SaveCompany(const CompanyFullInfo& company) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());

        txn.exec_params(
            "INSERT INTO companies(ticker, name, country, "
            "sector, industry, currency, description, exchange) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8) "
            "ON CONFLICT (ticker, exchange) DO UPDATE "
            "SET name = EXCLUDED.name, country = EXCLUDED.country, "
            "sector = EXCLUDED.sector, industry = EXCLUDED.industry, "
            "description = EXCLUDED.description, updated_at = NOW()",
            company.ticker, company.name, company.country.value_or(""),
            company.sector.value_or(""), company.industry.value_or(""), 
            company.currency, company.description.value_or(""), 
            company.exchange
        );

        txn.commit();
        Logger::Debug("[StockRepository] Company saved: {}", company.ticker);
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Failed to save company {}: {}", 
                        company.ticker, ex.what());
    }
}

void StockRepository::SaveCompaniesBatch(const std::vector<CompanyFullInfo>& companies) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());
        
        txn.exec(
            "CREATE TEMP TABLE temp_companies ON COMMIT DROP AS "
            "SELECT ticker, name, country, sector, industry, currency, description, exchange, updated_at "
            "FROM companies "
            "LIMIT 0"
        );

        auto stream = pqxx::stream_to::table(
                            txn,
                            {"temp_companies"},
                            {"ticker", "name", "country", "sector", "industry",
                                "currency", "description", "exchange", "updated_at"}
                        );
        
        for (const auto& comp : companies) {
            stream << std::make_tuple(comp.ticker, comp.name, comp.country, comp.sector,
                                        comp.industry, comp.currency, comp.description, comp.exchange, 
                                        TimeUtils::TimestampToString(comp.updated_at));
        }

        stream.complete();

        txn.exec(
            "INSERT INTO companies(ticker, name, country, sector, industry, currency, "
            "description, exchange, updated_at) "
            "SELECT * FROM temp_companies "
            "ON CONFLICT (ticker, exchange) DO UPDATE "
            "SET name = EXCLUDED.name, country = EXCLUDED.country, "
            "sector = EXCLUDED.sector, industry = EXCLUDED.industry, "
            "description = EXCLUDED.description, updated_at = NOW()"
        );

        txn.commit();
        Logger::Debug("[StockRepository] Batch insert companies completed");
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Batch insert companies failed: {}", ex.what());
    }
}

void StockRepository::SaveCompanyFinancialReport(const int company_id, 
                                                    const CompanyFinancialReport& fin_rep) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());

        txn.exec_params(
            "INSERT INTO companies_financial_reports(company_id, " 
            "period_date, report_type, currency, revenue, net_income, "
            "total_debt, equity) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8) "
            "ON CONFLICT (company_id, period_date, report_type) DO NOTHING",
            company_id, TimeUtils::DateToString(fin_rep.period_date), 
            ReportTypeToString(fin_rep.report_type), fin_rep.currency, 
            fin_rep.revenue, fin_rep.net_income, fin_rep.total_debt, fin_rep.equity
        );

        txn.commit();
        Logger::Debug("[StockRepository] Financial report for company_id {} saved", company_id);
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Failed to save financial report for company_id {}: {}",
                        company_id, ex.what());
    }
}

void StockRepository::SaveCompanyFinancialReportsBatch(const int company_id,
                                                        const std::vector<CompanyFinancialReport>& fin_reps) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());

        txn.exec(
            "CREATE TEMP TABLE temp_financials ON COMMIT DROP AS "
            "SELECT company_id, period_date, report_type, currency, revenue, net_income, total_debt, equity "
            "FROM companies_financial_reports "
            "LIMIT 0"
        );

        auto stream = pqxx::stream_to::table(
                            txn,
                            {"temp_financials"},
                            {"company_id", "period_date", "report_type",
                                "currency", "revenue", "net_income", 
                                "total_debt", "equity"}
                        );

        for (const auto& fin_rep : fin_reps) {
            stream << std::make_tuple(company_id, TimeUtils::DateToString(fin_rep.period_date),
                                        ReportTypeToString(fin_rep.report_type), fin_rep.currency, 
                                        fin_rep.revenue, fin_rep.net_income, fin_rep.total_debt, fin_rep.equity);
        }

        stream.complete();

        txn.exec(
            "INSERT INTO companies_financial_reports(company_id, period_date, report_type, "
            "currency, revenue, net_income, total_debt, equity) "
            "SELECT * FROM temp_financials "
            "ON CONFLICT (company_id, period_date, report_type) DO NOTHING"
        );

        txn.commit();
        Logger::Debug("[StockRepository] Batch insert company {} financial reports completed", company_id);
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Batch insert company {} financial reports failed", company_id);
    }
}

void StockRepository::SaveStockDividends(const int company_id, const StockDividends& dividends) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());
        
        std::optional<std::string> payment_date_;
        if (dividends.payment_date.has_value()) {
            payment_date_ = TimeUtils::DateToString(dividends.payment_date.value());
        }

        txn.exec_params(
            "INSERT INTO stock_dividends(company_id, ex_date, "
            "payment_date, amount) "
            "VALUES ($1, $2, $3, $4) "
            "ON CONFLICT (company_id, ex_date) DO NOTHING",
            company_id, TimeUtils::DateToString(dividends.ex_date), 
            payment_date_, dividends.amount
        );  

        txn.commit();
        Logger::Debug("[StockRepository] Stock dividends for company_id {} saved", company_id);
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Failed to save stock dividends for company_id {}: {}", 
                        company_id, ex.what());
    }
}

void StockRepository::SaveStockDividendsBatch(const int company_id, 
                                                const std::vector<StockDividends>& dividends) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());

        txn.exec(
            "CREATE TEMP TABLE temp_dividends ON COMMIT DROP AS " 
            "SELECT company_id, ex_date, payment_date, amount "
            "FROM stock_dividends "
            "LIMIT 0"
        );

        auto stream = pqxx::stream_to::table(
                            txn,
                            {"temp_dividends"},
                            {"company_id", "ex_date", "payment_date", "amount"} 
                        );

        for (const auto& div : dividends) {
            std::optional<std::string> pay_str;
            if (div.payment_date.has_value()) {
                pay_str = TimeUtils::DateToString(div.payment_date.value());
            }

            stream << std::make_tuple(company_id, TimeUtils::DateToString(div.ex_date),
                                        pay_str, div.amount);
        }    

        stream.complete();

        txn.exec(
            "INSERT INTO stock_dividends(company_id, ex_date, payment_date, amount) "
            "SELECT * FROM temp_dividends "
            "ON CONFLICT (company_id, ex_date) DO UPDATE "
            "SET payment_date = EXCLUDED.payment_date, amount = EXCLUDED.amount"
        );

        txn.commit();
        Logger::Debug("[StockRepository] Batch insert company {} stock dividends completed", company_id);
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Batch insert company {} dividends failed: {}", company_id, ex.what());
    }
}

void StockRepository::SaveStockPrice(const int company_id, const StockPriceCandle& stock_price) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());

        txn.exec_params(
            "INSERT INTO stock_prices(company_id, timestamp, "
            "open, high, low, close, volume) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7) "
            "ON CONFLICT (company_id, timestamp) DO NOTHING",
            company_id, TimeUtils::TimestampToString(stock_price.timestamp), 
            stock_price.open, stock_price.high, stock_price.low, 
            stock_price.close, stock_price.volume
        );

        txn.commit();
        Logger::Debug("[StockRepository] Stock price for company_id {} saved", company_id);
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Failed to save stock price for company_id {}: {}",
                        company_id, ex.what());
    }
}

void StockRepository::SaveStockPricesBatch(const int company_id, 
                                            const std::vector<StockPriceCandle>& prices) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());

        txn.exec(
            "CREATE TEMP TABLE temp_stock_prices ON COMMIT DROP AS "
            "SELECT company_id, timestamp, open, high, low, close, volume "
            "FROM stock_prices "
            "LIMIT 0"
        );

        auto stream = pqxx::stream_to::table(
                            txn, 
                            {"temp_stock_prices"}, 
                            {"company_id", "timestamp", "open", "high", "low", "close", "volume"}
                        );

        for (const auto& candle : prices) {
            stream << std::make_tuple(company_id, TimeUtils::TimestampToString(candle.timestamp),
                                        candle.open, candle.high, candle.low, candle.close, 
                                        candle.volume);
        }

        stream.complete();

        txn.exec(
            "INSERT INTO stock_prices(company_id, timestamp, open, high, low, close, volume) "
            "SELECT * FROM temp_stock_prices "
            "ON CONFLICT (company_id, timestamp) DO NOTHING"
        );

        txn.commit();
        Logger::Debug("[StockRepository] Batch insert prices for company {} completed", company_id);
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Batch insert prices failed for company {}: {}", company_id, ex.what());
    }
}

void StockRepository::SaveStockSplit(const int company_id, const StockSplit& split) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());

        txn.exec_params(
            "INSERT INTO stock_splits(company_id, split_date, numerator, denominator) "
            "VALUES ($1, $2, $3, $4) "
            "ON CONFLICT (company_id, split_date) DO UPDATE "
            "SET numerator = EXCLUDED.numerator, denominator = EXCLUDED.denominator",
            company_id, TimeUtils::DateToString(split.date), split.numerator, split.denominator
        );

        txn.commit();
        Logger::Debug("[StockRepository] Stock split saved for company_id: {}", company_id);
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Failed to save stock split for company_id {}: {}", company_id, ex.what());
    }
}

void StockRepository::SaveStockSplitsBatch(const int company_id, 
                                            const std::vector<StockSplit>& splits) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());

        txn.exec(
            "CREATE TEMP TABLE temp_splits ON COMMIT DROP AS "
            "SELECT company_id, split_date, numerator, denominator "
            "FROM stock_splits "
            "LIMIT 0"
        );

        auto stream = pqxx::stream_to::table(
                            txn,
                            {"temp_splits"},
                            {"company_id", "split_date", "numerator", "denominator"}
                        );

        for (const auto& split : splits) {
            stream << std::make_tuple(company_id, TimeUtils::DateToString(split.date),
                                        split.numerator, split.denominator);
        }
        
        stream.complete();

        txn.exec(
            "INSERT INTO stock_splits(company_id, split_date, numerator, denominator) "
            "SELECT * FROM temp_splits "
            "ON CONFLICT (company_id, split_date) DO UPDATE "
            "SET numerator = EXCLUDED.numerator, denominator = EXCLUDED.denominator"
        );

        txn.commit();
        Logger::Debug("[StockRepository] Batch insert stock splits completed for company_id: {}", company_id);
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Batch insert stock splits failed for company_id {}: {}", 
                        company_id, ex.what());
    }
}

std::vector<StockPriceCandle> StockRepository::GetHistoryStockPrices(const int company_id, 
                                                                        const Timestamp from, 
                                                                        const Timestamp to) {
    std::vector<StockPriceCandle> result;
    try {
        auto conn_guard = db_.GetConnection();

        pqxx::nontransaction ntxn(conn_guard->Get());

        std::string sql = R"(
            SELECT timestamp, open, high, low, close, volume 
            FROM stock_prices 
            WHERE company_id = $1 
            AND timestamp >= $2 
            AND timestamp <= $3 
            ORDER BY timestamp ASC
        )";

        auto rows = ntxn.exec_params(
                        sql, 
                        company_id, 
                        TimeUtils::TimestampToString(from), 
                        TimeUtils::TimestampToString(to)
                    );

        result.reserve(rows.size());
        for (const auto& row : rows) {
            StockPriceCandle candle;
            candle.timestamp = TimeUtils::StringToTimestamp(row[0].c_str());
            candle.open = row[1].as<double>();
            candle.high = row[2].as<double>();
            candle.low = row[3].as<double>();
            candle.close = row[4].as<double>();
            candle.volume = row[5].as<long long>(); 
            result.push_back(candle);
        }
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] GetHistoryStockCandles failed: {}", ex.what());
    }
    return result;
}

std::optional<int> StockRepository::GetCompanyId(const std::string& ticker) {
    try {
        auto conn_guard = db_.GetConnection();

        pqxx::nontransaction ntxn(conn_guard->Get());

        auto row = ntxn.exec_params1(
            "SELECT id "
            "FROM companies "
            "WHERE ticker = $1", 
            ticker
        );

        return row[0].as<int>();
    } catch (const std::exception& ex) {
        return std::nullopt;
    }
}

std::optional<CompanyFullInfo> StockRepository::GetCompanyByTicker(const std::string& ticker) {
    try {
        auto conn_guard = db_.GetConnection();

        pqxx::nontransaction ntxn(conn_guard->Get());
        
        auto row = ntxn.exec_params1(
            "SELECT id, ticker, name, country, sector, industry, currency, "
            "description, exchange, updated_at "
            "FROM companies "
            "WHERE ticker = $1", 
            ticker
        );

        CompanyFullInfo info;
        info.id = row[0].as<int>();
        info.ticker = row[1].as<std::string>();
        info.name = row[2].as<std::string>();
        if (!row[3].is_null()) {
            info.country = row[3].as<std::string>();
        }
        if (!row[4].is_null()) {
            info.sector = row[4].as<std::string>();
        }
        if (!row[5].is_null()) {
            info.industry = row[5].as<std::string>();
        }
        info.currency = row[6].as<std::string>();
        if (!row[7].is_null()) {
            info.description = row[7].as<std::string>();
        }
        info.exchange = row[8].as<std::string>();
        info.updated_at = TimeUtils::StringToTimestamp(row[9].c_str());
        
        return info;
    } catch (const std::exception& ex) {
        return std::nullopt;
    }
}

std::vector<CompanyPreview> StockRepository::GetAllCompaniesPreview() {
    std::vector<CompanyPreview> result;
    try {
        auto conn_guard = db_.GetConnection();

        pqxx::nontransaction ntxn(conn_guard->Get());

        auto rows = ntxn.exec_params(
            "SELECT id, ticker, name, country, sector, currency "
            "FROM companies "
            "ORDER BY ticker"
        );

        for (const auto& row : rows) {
            CompanyPreview cp;
            cp.id = row[0].as<int>();
            cp.ticker = row[1].as<std::string>();
            cp.name = row[2].as<std::string>();
            if (!row[3].is_null()) {
                cp.country = row[3].as<std::string>();
            }
            if (!row[4].is_null()) {
                cp.sector = row[4].as<std::string>();
            }
            cp.currency = row[5].as<std::string>();
            
            result.push_back(cp);
        }
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Failed to get all companies: {}", ex.what());
    }
    return result;
}

std::optional<StockPriceCandle> StockRepository::GetLastStockPrice(int company_id) {
    try {
        auto conn_guard = db_.GetConnection();

        pqxx::nontransaction ntxn(conn_guard->Get());
        
        auto rows = ntxn.exec_params(
            "SELECT timestamp, open, high, low, close, volume "
            "FROM stock_prices "
            "WHERE company_id = $1 "
            "ORDER BY timestamp DESC "
            "LIMIT 1",
            company_id
        );

        if (rows.empty()) {
            return std::nullopt;
        }

        auto row = rows[0];
        return StockPriceCandle{
            TimeUtils::StringToTimestamp(row[0].c_str()),
            row[1].as<double>(),
            row[2].as<double>(),
            row[3].as<double>(),
            row[4].as<double>(),
            row[5].as<long long>()
        };
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Failed to get last stock price for {}: {}", company_id, ex.what());
        return std::nullopt;
    }
}

std::vector<StockDividends> StockRepository::GetDividends(int company_id, const Date from, const Date to) {
    std::vector<StockDividends> result;
    try {
        auto conn_guard = db_.GetConnection();

        pqxx::nontransaction ntxn(conn_guard->Get());
        
        auto rows = ntxn.exec_params(
            "SELECT ex_date, payment_date, amount "
            "FROM stock_dividends "
            "WHERE company_id = $1 AND ex_date >= $2 AND ex_date <= $3 "
            "ORDER BY ex_date DESC",
            company_id, TimeUtils::DateToString(from), TimeUtils::DateToString(to)
        );

        for (const auto& row : rows) {
            StockDividends div;
            div.ex_date = TimeUtils::StringToDate(row[0].c_str());
            if (row[1].is_null()) {
                div.payment_date = std::nullopt;
            } else {
                div.payment_date = TimeUtils::StringToDate(row[1].c_str());
            }
            div.amount = row[2].as<double>();
            result.push_back(div);
        }
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Failed to get dividends: {}", ex.what());
    }
    return result;
}

std::vector<CompanyFinancialReport> StockRepository::GetFinancialReports(int company_id) {
    std::vector<CompanyFinancialReport> reports;
    try {
        auto conn_guard = db_.GetConnection();

        pqxx::nontransaction ntxn(conn_guard->Get());

        auto rows = ntxn.exec_params(
            "SELECT period_date, report_type, currency, revenue, net_income, total_debt, equity "
            "FROM companies_financial_reports "
            "WHERE company_id = $1 "
            "ORDER BY period_date DESC",
            company_id
        );

        for (const auto& row : rows) {
            CompanyFinancialReport r;

            r.period_date = TimeUtils::StringToDate(row[0].c_str());
            r.report_type = StringToReportType(row[1].c_str());
            r.currency = row[2].as<std::string>();
            r.revenue = row[3].as<double>();
            r.net_income = row[4].as<double>();
            r.total_debt = row[5].as<double>();
            r.equity = row[6].as<double>();

            reports.push_back(r);
        }
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Failed to get financial reports: {}", ex.what());
    }
    return reports;
}

void StockRepository::UpdateCompanyDescription(int company_id, const std::string& description) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());
        
        txn.exec_params(
            "UPDATE companies "
            "SET description = $1, updated_at = NOW() "
            "WHERE id = $2",
            description, company_id
        );

        txn.commit();
        Logger::Debug("[StockRepository] Updated description for company_id: {}", company_id);
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Failed to update company {}: {}", company_id, ex.what());
    }
}

void StockRepository::DeleteCompany(int company_id) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());

        txn.exec_params(
            "DELETE FROM companies "
            "WHERE id = $1", 
            company_id
        );
        txn.commit();
        Logger::Info("[StockRepository] Deleted company_id: {}", company_id);
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Failed to delete company {}: {}", company_id, ex.what());
    }
}

void StockRepository::DeleteOldPrices(int company_id, const Timestamp older_than) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());

        txn.exec_params(
            "DELETE FROM stock_prices "
            "WHERE company_id = $1 AND timestamp < $2",
            company_id, TimeUtils::TimestampToString(older_than)
        );

        txn.commit();
        Logger::Info("[StockRepository] Deleted company {} prices older than {}", 
                        company_id, TimeUtils::TimestampToString(older_than));
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Failed to cleanup prices for {} older than {}: {}", 
                        company_id, TimeUtils::TimestampToString(older_than), ex.what());
    }
}

std::optional<Timestamp> StockRepository::GetLastUpdate(int company_id, const std::string& data_type) {
    try {
        auto conn_guard = db_.GetConnection();

        pqxx::nontransaction ntxn(conn_guard->Get());
        
        auto row = ntxn.exec_params1(
            "SELECT updated_at FROM data_update_logs "
            "WHERE company_id = $1 AND data_type = $2",
            company_id, data_type
        );
        
        return TimeUtils::StringToTimestamp(row[0].c_str());
    } catch (...) {
        return std::nullopt;
    }
}

void StockRepository::SetLastUpdate(int company_id, const std::string& data_type) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());
        
        txn.exec_params(
            "INSERT INTO data_update_logs (company_id, data_type, updated_at) "
            "VALUES ($1, $2, NOW()) "
            "ON CONFLICT (company_id, data_type) "
            "DO UPDATE SET updated_at = NOW()",
            company_id, data_type
        );
        
        txn.commit();
    } catch (const std::exception& ex) {
        Logger::Error("[StockRepository] Failed to set last update log: {}", ex.what());
    }
}

std::string StockRepository::ReportTypeToString(const ReportType type) {
    switch (type) {
        case ReportType::Annual: 
            return "annual";
        case ReportType::Quarterly:
            return "quarterly";
        case ReportType::Ttm:
            return "ttm";
    }
    Logger::Error("[StockRepository] Couldn't convert ReportType type to string");
    throw std::runtime_error("Couldn't convert ReportType type to string");
}

ReportType StockRepository::StringToReportType(const std::string& str) {
    if (str == "annual") {
        return ReportType::Annual;
    } else if (str == "quarterly") {
        return ReportType::Quarterly;
    } else if (str == "ttm") {
        return ReportType::Ttm;
    }
    
    Logger::Error("[StockRepository] Couldn't convert string type to ReportType: {}", str);
    throw std::runtime_error("Couldn't convert string '" + str + "' to ReportType");
}