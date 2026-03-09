#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <optional>

using Timestamp = std::chrono::system_clock::time_point;
using Date = std::chrono::year_month_day;

enum class ReportType {
    Annual, 
    Quarterly, 
    Ttm
};

enum class TimeFrame {
    Minute1,
    Minute5,
    Minute15,
    Hourly,
    Hour2,
    Hour6,
    Hour12,
    Daily,
    Weekly,
    Monthly
};

struct TickerSearchResult {
    std::string name;
    std::string ticker;
    std::string type;   
    std::string region;
};

struct CompanyPreview {
    int id;
    std::string ticker;
    std::string name;
    std::optional<std::string> country;
    std::optional<std::string> sector;
    std::string currency;
};

struct CompanyFullInfo {
    int id;
    std::string ticker;
    std::string name;
    std::optional<std::string> country;
    std::optional<std::string> sector;
    std::optional<std::string> industry;
    std::string currency;
    std::optional<std::string> description;
    std::string exchange;
    Timestamp updated_at;
};

struct CompanyFinancialReport {
    Date period_date;
    ReportType report_type;
    std::string currency;
    double revenue;
    double net_income;
    double total_debt;
    double equity;
};

struct StockPriceCandle {
    Timestamp timestamp; 
    
    double open;
    double high;
    double low;
    double close;
    long long volume;
};

struct StockSplit {
    Date date;
    double numerator;
    double denominator;
};

struct StockDividends {
    Date ex_date;
    std::optional<Date> payment_date;
    double amount;
};

struct CryptoAsset {
    int id;
    std::string ticker;
    std::string name;
    Timestamp updated_at;
};

struct CryptoPriceCandle {
    Timestamp timestamp;

    double open;
    double high;
    double low;
    double close;
    double volume;
};

template <typename T>
struct PaginatedResponse {
    std::vector<T> data;
    bool has_more;
    int next_offset;
    int total_count;
};

struct MarketNews {
    Timestamp datetime;
    std::string headline;
    std::string source;
    std::string url;
    std::string summary;
    double sentiment_score;
};

struct InsiderTransaction {
    Date transaction_date;
    std::string owner_name;    
    std::string transaction_type;
    double amount;         
    double price;      
};

struct AnalystRating {
    Date date;
    int buy;     
    int hold;   
    int sell;     
    double target_price; 
};

struct EconomicIndicator {
    Date date;
    double value;
};

struct CalendarEvent {
    Date date;
    std::string ticker;
    std::string event_type;
    std::optional<double> estimate;
};


struct GlobalCryptoMetrics {
    double total_market_cap_usd; 
    double total_volume_24h;     
    double btc_dominance_percentage;
    int active_cryptocurrencies;  
};

struct OrderBookEntry {
    double price;
    double amount; 
};

struct OrderBook {
    std::string ticker;
    std::vector<OrderBookEntry> bids;
    std::vector<OrderBookEntry> asks; 
};