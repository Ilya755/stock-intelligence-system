#include "CryptoRepository.hpp"

#include <format>
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

PgParam Param(double value) {
    return std::format("{}", value);
}

PgParam Param(std::string value) {
    return value;
}

}

CryptoRepository::CryptoRepository(Database& db)
    : db_(db)
    {}

boost::asio::awaitable<void> CryptoRepository::SaveCryptoAssetAsync(
        CryptoAsset crypto_asset) {
    try {
        co_await db_.Query(
            "INSERT INTO crypto_assets(ticker, name, updated_at) "
            "VALUES ($1, $2, NOW()) "
            "ON CONFLICT (ticker) DO UPDATE "
            "SET name = EXCLUDED.name, updated_at = NOW()",
            {Param(crypto_asset.ticker), Param(crypto_asset.name)});

        Logger::Debug(
            "[CryptoRepository] Cryptocurrency asset {} saved", 
            crypto_asset.ticker);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CryptoRepository] Failed to save cryptocurrency asset {}: {}",
            crypto_asset.ticker, ex.what());
    }
}

boost::asio::awaitable<void> CryptoRepository::SaveCryptoAssetsBatchAsync(
        std::vector<CryptoAsset> crypto_assets) {
    if (crypto_assets.empty()) {
        co_return;
    }

    try {
        nlohmann::json rows = nlohmann::json::array();
        const auto now = TimeUtils::TimestampToString(std::chrono::system_clock::now());
        for (const auto& asset : crypto_assets) {
            rows.push_back({
                {"ticker", asset.ticker},
                {"name", asset.name},
                {"updated_at", now}
            });
        }

        co_await db_.Query(
            "INSERT INTO crypto_assets(ticker, name, updated_at) "
            "SELECT ticker, name, updated_at "
            "FROM jsonb_to_recordset($1::jsonb) "
            "AS x(ticker text, name text, updated_at timestamp) "
            "ON CONFLICT (ticker) DO UPDATE "
            "SET name = EXCLUDED.name, updated_at = EXCLUDED.updated_at",
            {Param(rows.dump())});

        Logger::Debug(
            "[CryptoRepository] Batch insert cryptocurrency assets completed");
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CryptoRepository] Batch insert cryptocurrency assets failed: {}", 
            ex.what());
    }
}

boost::asio::awaitable<void> CryptoRepository::SaveCryptoPriceAsync(
        int asset_id, 
        CryptoPriceCandle crypto_price) {
    try {
        co_await db_.Query(
            "INSERT INTO crypto_prices(asset_id, timestamp, open, high, low, close, volume) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7) "
            "ON CONFLICT (asset_id, timestamp) DO NOTHING",
            {
                Param(asset_id),
                Param(TimeUtils::TimestampToString(crypto_price.timestamp)),
                Param(crypto_price.open),
                Param(crypto_price.high),
                Param(crypto_price.low),
                Param(crypto_price.close),
                Param(crypto_price.volume)
            });

        Logger::Debug(
            "[CryptoRepository] Price of cryptocurrnecy asset with id {} saved", 
            asset_id);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CryptoRepository] Failed to save price of cryptocurrency asset with id {}: {}",
            asset_id, ex.what());
    }
}

boost::asio::awaitable<void> CryptoRepository::SaveCryptoPricesBatchAsync(
        int asset_id, 
        std::vector<CryptoPriceCandle> prices) {
    try {
        nlohmann::json rows = nlohmann::json::array();
        for (const auto& candle : prices) {
            rows.push_back({
                {"asset_id", asset_id},
                {"timestamp", TimeUtils::TimestampToString(candle.timestamp)},
                {"open", candle.open},
                {"high", candle.high},
                {"low", candle.low},
                {"close", candle.close},
                {"volume", candle.volume}
            });
        }

        co_await db_.Query(
            "INSERT INTO crypto_prices(asset_id, timestamp, open, high, low, close, volume) "
            "SELECT asset_id, timestamp, open, high, low, close, volume "
            "FROM jsonb_to_recordset($1::jsonb) "
            "AS x(asset_id int, timestamp timestamp, open numeric, high numeric, "
            "low numeric, close numeric, volume numeric) "
            "ON CONFLICT (asset_id, timestamp) DO NOTHING",
            {Param(rows.dump())});

        Logger::Debug(
            "[CryptoRepository] Batch insert for asset_id {} finished", 
            asset_id);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CryptoRepository] Batch insert failed for asset_id {}: {}", 
            asset_id, ex.what());
    }
}

boost::asio::awaitable<std::optional<int>> CryptoRepository::GetCryptoAssetIdAsync(
        std::string ticker) {
    try {
        auto result = co_await db_.Query(
            "SELECT id "
            "FROM crypto_assets "
            "WHERE ticker = $1",
            {Param(std::move(ticker))});
        if (result.RowCount() == 0) {
            co_return std::nullopt;
        }

        co_return std::stoi(Text(result, 0, 0));
    } catch (const std::exception&) {
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::optional<CryptoAsset>> CryptoRepository::GetCryptoAssetByTickerAsync(
        std::string ticker) {
    try {
        auto result = co_await db_.Query(
            "SELECT id, ticker, name, updated_at "
            "FROM crypto_assets "
            "WHERE ticker = $1",
            {Param(std::move(ticker))});
        if (result.RowCount() == 0) {
            co_return std::nullopt;
        }

        CryptoAsset asset;
        asset.id = std::stoi(Text(result, 0, 0));
        asset.ticker = Text(result, 0, 1);
        if (!result.IsNull(0, 2)) {
            asset.name = Text(result, 0, 2);
        }
        asset.updated_at = TimeUtils::StringToTimestamp(Text(result, 0, 3));

        co_return asset;
    } catch (const std::exception&) {
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::vector<CryptoAsset>> CryptoRepository::GetAllCryptoAssetsAsync() {
    std::vector<CryptoAsset> assets;
    try {
        auto result = co_await db_.Query(
            "SELECT id, ticker, name, updated_at "
            "FROM crypto_assets "
            "ORDER BY ticker");

        assets.reserve(static_cast<std::size_t>(result.RowCount()));
        
        for (int row = 0; row < result.RowCount(); ++row) {
            CryptoAsset asset;
            asset.id = std::stoi(Text(result, row, 0));
            asset.ticker = Text(result, row, 1);
            if (!result.IsNull(row, 2)) {
                asset.name = Text(result, row, 2);
            }
            asset.updated_at = TimeUtils::StringToTimestamp(Text(result, row, 3));
            assets.push_back(std::move(asset));
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CryptoRepository] Failed to get all crypto assets: {}", 
            ex.what());
    }
    co_return assets;
}

boost::asio::awaitable<std::optional<CryptoPriceCandle>> CryptoRepository::GetLastCryptoPriceAsync(
        int asset_id) {
    try {
        auto result = co_await db_.Query(
            "SELECT timestamp, open, high, low, close, volume "
            "FROM crypto_prices "
            "WHERE asset_id = $1 "
            "ORDER BY timestamp DESC "
            "LIMIT 1",
            {Param(asset_id)});
        if (result.RowCount() == 0) {
            co_return std::nullopt;
        }

        co_return CryptoPriceCandle{
            TimeUtils::StringToTimestamp(Text(result, 0, 0)),
            std::stod(Text(result, 0, 1)),
            std::stod(Text(result, 0, 2)),
            std::stod(Text(result, 0, 3)),
            std::stod(Text(result, 0, 4)),
            std::stod(Text(result, 0, 5))
        };
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CryptoRepository] Failed to get last crypto price for {}: {}", 
            asset_id, ex.what());
        co_return std::nullopt;
    }
}

boost::asio::awaitable<std::vector<CryptoPriceCandle>> CryptoRepository::GetCryptoPricesHistoryAsync(
        int asset_id, 
        Timestamp from, 
        Timestamp to) {
    std::vector<CryptoPriceCandle> prices;
    try {
        auto result = co_await db_.Query(
            "SELECT timestamp, open, high, low, close, volume "
            "FROM crypto_prices "
            "WHERE asset_id = $1 AND timestamp >= $2 AND timestamp <= $3 "
            "ORDER BY timestamp ASC",
            {
                Param(asset_id),
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
                std::stod(Text(result, row, 5))
            });
        }
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CryptoRepository] Failed to get crypto prices history for {}: {}", 
            asset_id, ex.what());
    }
    co_return prices;
}

boost::asio::awaitable<void> CryptoRepository::UpdateCryptoAssetNameAsync(
        int id, 
        std::string new_name) {
    try {
        co_await db_.Query(
            "UPDATE crypto_assets "
            "SET name = $1 "
            "WHERE id = $2",
            {Param(std::move(new_name)), Param(id)});

        Logger::Debug(
            "[CryptoRepository] Updated name for crypto_asset_id {}: {}", 
            id, new_name);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CryptoRepository] Failed to update name crypto_asset {}: {}", 
            id, ex.what());
    }
}

boost::asio::awaitable<void> CryptoRepository::DeleteCryptoAssetAsync(int id) {
    try {
        co_await db_.Query(
            "DELETE FROM crypto_assets "
            "WHERE id = $1", 
            {Param(id)});

        Logger::Info(
            "[CryptoRepository] Deleted crypto_asset: {}", 
            id);
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CryptoRepository] Failed to delete crypto_asset {}: {}", 
            id, ex.what());
    }
}

boost::asio::awaitable<void> CryptoRepository::DeleteOldCryptoPricesAsync(
        int asset_id, 
        Timestamp older_than) {
    try {
        const auto timestamp = TimeUtils::TimestampToString(older_than);
        co_await db_.Query(
            "DELETE FROM crypto_prices "
            "WHERE asset_id = $1 AND timestamp < $2",
            {Param(asset_id), Param(timestamp)});

        Logger::Info(
            "[CryptoRepository] Deleted crypto prices of asset_id {} older than {}", 
            asset_id, TimeUtils::TimestampToString(older_than));
    } catch (const std::exception& ex) {
        Logger::Error(
            "[CryptoRepository] Failed to delete crypto prices for {} older than {}: {}", 
            asset_id, TimeUtils::TimestampToString(older_than), ex.what());
    }
}