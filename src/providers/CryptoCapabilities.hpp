#pragma once

#include <string>
#include <map>
#include <optional>

enum class CryptoCapability {
    RealtimePrice,     
    History,         
    Metadata,       
    GlobalMetrics,   
    TopList,      
    OrderBook,      
    Exchanges,     
    Trades,    
    FearGreedIndex     
};

std::optional<CryptoCapability> StringToCryptoCapability(const std::string& str);