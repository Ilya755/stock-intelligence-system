#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_set>
#include <algorithm>
#include <map>
#include <chrono>

#include "../ICryptoProvider.hpp"
#include "../BaseProvider.hpp"
#include "../CryptoCapabilities.hpp"
#include "../../common/TimeUtils.hpp"

class CoinCapProvider : public ICryptoProvider, public BaseProvider {
public:
    CoinCapProvider(std::shared_ptr<IHttpClient> client,
                        const std::string& name,
                        const std::string& api_key,
                        const std::string& base_url,
                        const Limits& limits,
                        const std::vector<std::string>& config_caps);

    std::string GetName() const override;

    bool HasCapability(const CryptoCapability cap) const override;

    std::optional<double> GetCryptoPrice(const std::string& ticker) override;

    std::optional<CryptoAsset> GetCryptoAssetInfo(const std::string& ticker) override;

    std::vector<CryptoAsset> GetCryptoTopList(const int limit) override;

    std::vector<CryptoPriceCandle> GetCryptoHistory(const std::string& ticker, 
                                                        const Timestamp from, const Timestamp to, 
                                                        const TimeFrame interval) override;

    std::vector<CryptoAsset> SearchAsset(const std::string& query) override;

    std::optional<GlobalCryptoMetrics> GetGlobalMetrics() override;

    std::optional<OrderBook> GetOrderBook(const std::string&, const int) override;

private:
    std::string name_;
    std::unordered_set<CryptoCapability> crypto_caps_;

    std::string ConvertInterval(const TimeFrame tf);
};