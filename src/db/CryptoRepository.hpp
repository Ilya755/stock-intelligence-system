#pragma once

#include <vector>
#include <string>
#include <optional>

#include "boost/asio/awaitable.hpp"

#include "../domain/Entities.hpp"
#include "Database.hpp"

class CryptoRepository {
public:
    explicit CryptoRepository(Database& db);

    boost::asio::awaitable<void> SaveCryptoAssetAsync(CryptoAsset crypto_asset);

    boost::asio::awaitable<void> SaveCryptoAssetsBatchAsync(std::vector<CryptoAsset> crypto_assets);

    boost::asio::awaitable<void> SaveCryptoPriceAsync(
        int asset_id, 
        CryptoPriceCandle crypto_price);

    boost::asio::awaitable<void> SaveCryptoPricesBatchAsync(
        int asset_id, 
        std::vector<CryptoPriceCandle> prices);

    boost::asio::awaitable<std::optional<int>> GetCryptoAssetIdAsync(std::string ticker);

    boost::asio::awaitable<std::optional<CryptoAsset>> GetCryptoAssetByTickerAsync(std::string ticker);

    boost::asio::awaitable<std::vector<CryptoAsset>> GetAllCryptoAssetsAsync();

    boost::asio::awaitable<std::optional<CryptoPriceCandle>> GetLastCryptoPriceAsync(int asset_id);

    boost::asio::awaitable<std::vector<CryptoPriceCandle>> GetCryptoPricesHistoryAsync(
        int asset_id,
        Timestamp from, 
        Timestamp to);

    boost::asio::awaitable<void> UpdateCryptoAssetNameAsync(int id, std::string new_name);

    boost::asio::awaitable<void> DeleteCryptoAssetAsync(int id);

    boost::asio::awaitable<void> DeleteOldCryptoPricesAsync(int asset_id, Timestamp older_than);

private:
    Database& db_;
};