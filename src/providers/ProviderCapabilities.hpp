#pragma once

#include <string> 
#include <map>
#include <optional>

enum class ProviderCapability {
    PriceRealtime,     
    PriceIntraday,     
    PriceDaily,      
    PriceHistoryDeep, 
    FinancialsBasic,  
    FinancialsDeep,  
    FinancialRatios, 
    CompanyProfile,  
    Dividends,       
    Earnings,        
    StockSplits,     
    TechIndicators,  
    AnalystRatings,  
    News,            
    Sentiment,       
    Insiders,        
    Institutional,   
    MacroEconomics   
};

enum class TechIndicatorType { 
    RSI, 
    MACD, 
    SMA, 
    EMA, 
    BollingerBands 
};

enum class MacroIndicatorType { 
    GDP, 
    Inflation, 
    Unemployment, 
    FedRate 
};

std::optional<ProviderCapability> StringToProviderCapability(const std::string& str);