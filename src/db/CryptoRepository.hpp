#pragma once

#include <vector>
#include <string>
#include <tuple>
#include <optional>

#include "../domain/Entities.hpp"
#include "Database.hpp"

class CryptoRepository {
public:
    explicit CryptoRepository(Database& db);

    void SaveCryptoAsset(const CryptoAsset& crypto_asset);

    void SaveCryptoAssetsBatch(const std::vector<CryptoAsset>& crypto_assets);

    void SaveCryptoPrice(const int asset_id, const CryptoPriceCandle& crypto_price);

    void SaveCryptoPricesBatch(const int asset_id, const std::vector<CryptoPriceCandle>& prices);

    std::optional<int> GetCryptoAssetId(const std::string& ticker);

    std::optional<CryptoAsset> GetCryptoAssetByTicker(const std::string& ticker);

    std::vector<CryptoAsset> GetAllCryptoAssets();

    std::optional<CryptoPriceCandle> GetLastCryptoPrice(int asset_id);

    std::vector<CryptoPriceCandle> GetCryptoPricesHistory(int asset_id, const Timestamp from, const Timestamp to);

    void UpdateCryptoAssetName(int id, const std::string& new_name);

    void DeleteCryptoAsset(int id);

    void DeleteOldCryptoPrices(int asset_id, const Timestamp older_than);

private:
    Database& db_;
};