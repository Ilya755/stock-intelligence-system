#include "CryptoCapabilities.hpp"

std::optional<CryptoCapability> StringToCryptoCapability(const std::string& str) {
    static const std::map<std::string, CryptoCapability> map = {
        {"crypto_price_realtime", CryptoCapability::RealtimePrice},
        {"crypto_price_intraday", CryptoCapability::RealtimePrice},
        {"crypto_price_daily", CryptoCapability::RealtimePrice},
        {"crypto_price_weekly", CryptoCapability::RealtimePrice},
        {"crypto_price_monthly", CryptoCapability::RealtimePrice},
        {"crypto_avg_price", CryptoCapability::RealtimePrice},
        
        {"crypto_history", CryptoCapability::History},
        {"crypto_history_ohlcv", CryptoCapability::History},
        {"crypto_klines_ohlcv", CryptoCapability::History},
        {"crypto_market_chart", CryptoCapability::History},
        
        {"crypto_metadata", CryptoCapability::Metadata},
        {"crypto_coin_info", CryptoCapability::Metadata},
        {"crypto_asset_info", CryptoCapability::Metadata},
        {"crypto_symbol_info", CryptoCapability::Metadata},
        {"crypto_exchange_info", CryptoCapability::Metadata},
        
        {"crypto_global_metrics", CryptoCapability::GlobalMetrics},
        
        {"crypto_24hr_ticker", CryptoCapability::TopList},
        {"crypto_top_list", CryptoCapability::TopList},
        {"crypto_trending", CryptoCapability::TopList},
        {"crypto_assets_list", CryptoCapability::TopList},
        
        {"crypto_order_book", CryptoCapability::OrderBook},
        {"crypto_trades", CryptoCapability::Trades},
        
        {"crypto_exchanges", CryptoCapability::Exchanges},
        {"crypto_markets", CryptoCapability::Exchanges},
        
        {"crypto_fear_greed_index", CryptoCapability::FearGreedIndex},
        {"crypto_rates", CryptoCapability::GlobalMetrics}
    };

    auto it = map.find(str);
    if (it != map.end()) {
        return it->second;
    }
    
    return std::nullopt;
}