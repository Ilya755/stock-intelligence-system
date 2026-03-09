#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../domain/Entities.hpp"
#include "../common/TimeUtils.hpp"
#include "CryptoCapabilities.hpp"

class ICryptoProvider {
public:
    virtual ~ICryptoProvider() = default;

    virtual std::string GetName() const = 0;
    virtual bool HasCapability(const CryptoCapability cap) const = 0;

    virtual std::optional<double> GetCryptoPrice(const std::string& ticker) = 0;

    virtual std::optional<CryptoAsset> GetCryptoAssetInfo(const std::string& ticker) = 0;

    virtual std::vector<CryptoAsset> GetCryptoTopList(const int limit = 20) = 0;

    virtual std::optional<GlobalCryptoMetrics> GetGlobalMetrics() = 0;

    virtual std::vector<CryptoPriceCandle> GetCryptoHistory(const std::string& ticker, 
                                                                const Timestamp from, 
                                                                const Timestamp to, 
                                                                const TimeFrame interval) = 0;

    virtual std::optional<OrderBook> GetOrderBook(const std::string& ticker, const int depth = 10) = 0;

    virtual std::vector<CryptoAsset> SearchAsset(const std::string& query) = 0;
};