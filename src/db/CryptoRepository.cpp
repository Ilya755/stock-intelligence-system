#include "CryptoRepository.hpp"

#include <tuple>
#include <optional>
#include <vector>

#include "pqxx/pqxx"

#include "../common/TimeUtils.hpp"
#include "../common/Logger.hpp"

CryptoRepository::CryptoRepository(Database& db)
    : db_(db)
    {}

void CryptoRepository::SaveCryptoAsset(const CryptoAsset& crypto_asset) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());

        txn.exec_params(
            "INSERT INTO crypto_assets(ticker, name, updated_at) "
            "VALUES ($1, $2, NOW()) "
            "ON CONFLICT (ticker) DO UPDATE "
            "SET name = EXCLUDED.name, updated_at = NOW()",
            crypto_asset.ticker, crypto_asset.name
        );

        txn.commit();
        Logger::Debug("[CryptoRepository] Cryptocurrency asset {} saved", crypto_asset.ticker);
    } catch (const std::exception& ex) {
        Logger::Error("[CryptoRepository] Failed to save cryptocurrency asset {}: {}",
                        crypto_asset.ticker, ex.what());
    }
}

void CryptoRepository::SaveCryptoAssetsBatch(const std::vector<CryptoAsset>& crypto_assets) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());

        txn.exec(
            "CREATE TEMP TABLE temp_crypto_assets ON COMMIT DROP AS "
            "SELECT ticker, name, updated_at "
            "FROM crypto_assets "
            "LIMIT 0"
        );

        auto stream = pqxx::stream_to::table(
                            txn,
                            {"temp_crypto_assets"},
                            {"ticker", "name", "updated_at"}
                        );
        
        std::string now_str = TimeUtils::TimestampToString(std::chrono::system_clock::now());

        for (const auto& crypto_asset : crypto_assets) {
            stream << std::make_tuple(crypto_asset.ticker, crypto_asset.name, now_str);
        }

        stream.complete();

        txn.exec(
            "INSERT INTO crypto_assets (ticker, name, updated_at) "
            "SELECT * FROM temp_crypto_assets "
            "ON CONFLICT (ticker) DO UPDATE "
            "SET name = EXCLUDED.name, updated_at = EXCLUDED.updated_at"
        );

        txn.commit();
        Logger::Debug("[CryptoRepository] Batch insert cryptocurrency assets completed");
    } catch (const std::exception& ex) {
        Logger::Error("[CryptoRepository] Batch insert cryptocurrency assets failed: {}", ex.what());
    }
}

void CryptoRepository::SaveCryptoPrice(const int asset_id, const CryptoPriceCandle& crypto_price) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());

        txn.exec_params(
            "INSERT INTO crypto_prices(asset_id, timestamp, "
            "open, high, low, close, volume) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7) "
            "ON CONFLICT (asset_id, timestamp) DO NOTHING",
            asset_id, TimeUtils::TimestampToString(crypto_price.timestamp), 
            crypto_price.open, crypto_price.high, crypto_price.low, 
            crypto_price.close, crypto_price.volume
        );

        txn.commit();
        Logger::Debug("[CryptoRepository] Price of cryptocurrnecy asset with id {} saved", asset_id);
    } catch (const std::exception& ex) {
        Logger::Error("[CryptoRepository] Failed to save price of cryptocurrency asset with id {}: {}",
                        asset_id, ex.what());
    }
}

void CryptoRepository::SaveCryptoPricesBatch(const int asset_id, 
                                                const std::vector<CryptoPriceCandle>& prices) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());

        txn.exec(
            "CREATE TEMP TABLE temp_crypto_prices ON COMMIT DROP AS "
            "SELECT asset_id, timestamp, open, high, low, close, volume "
            "FROM crypto_prices "
            "LIMIT 0"
        );

        auto stream = pqxx::stream_to::table(
                            txn, 
                            {"temp_crypto_prices"}, 
                            {"asset_id", "timestamp", "open", "high", "low", "close", "volume"}
                        );

        for (const auto& candle : prices) {
            stream << std::make_tuple(asset_id, TimeUtils::TimestampToString(candle.timestamp),
                                        candle.open, candle.high, candle.low, candle.close, 
                                        candle.volume);
        }

        stream.complete();

        txn.exec(
            "INSERT INTO crypto_prices (asset_id, timestamp, open, "
            "high, low, close, volume) "
            "SELECT * FROM temp_crypto_prices "
            "ON CONFLICT (asset_id, timestamp) DO NOTHING"
        );

        txn.commit();
        Logger::Debug("[CryptoRepository] Batch insert for asset_id {} finished", asset_id);
    } catch (const std::exception& ex) {
        Logger::Error("[CryptoRepository] Batch insert failed for asset_id {}: {}", asset_id, ex.what());
    }
}

std::optional<int> CryptoRepository::GetCryptoAssetId(const std::string& ticker) {
    try {
        auto conn_guard = db_.GetConnection();

        pqxx::nontransaction ntxn(conn_guard->Get());

        auto row = ntxn.exec_params1(
            "SELECT id "
            "FROM crypto_assets "
            "WHERE ticker = $1", 
            ticker
        );

        return row[0].as<int>();
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<CryptoAsset> CryptoRepository::GetCryptoAssetByTicker(const std::string& ticker) {
    try {
        auto conn_guard = db_.GetConnection();

        pqxx::nontransaction ntxn(conn_guard->Get());

        auto row = ntxn.exec_params1(
            "SELECT id, ticker, name, updated_at "
            "FROM crypto_assets "
            "WHERE ticker = $1", 
            ticker
        );

        CryptoAsset asset;
        asset.id = row[0].as<int>();
        asset.ticker = row[1].as<std::string>();
        if (!row[2].is_null()) {
            asset.name = row[2].as<std::string>();
        }
        asset.updated_at = TimeUtils::StringToTimestamp(row[3].c_str());

        return asset;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::vector<CryptoAsset> CryptoRepository::GetAllCryptoAssets() {
    std::vector<CryptoAsset> result;
    try {
        auto conn_guard = db_.GetConnection();

        pqxx::nontransaction ntxn(conn_guard->Get());

        auto rows = ntxn.exec_params(
            "SELECT id, ticker, name, updated_at "
            "FROM crypto_assets "
            "ORDER BY ticker"
        );

        result.reserve(rows.size());
        for (const auto& row : rows) {
            CryptoAsset a;
            a.id = row[0].as<int>();
            a.ticker = row[1].as<std::string>();
            if (!row[2].is_null()) {
                a.name = row[2].as<std::string>();
            }
            a.updated_at = TimeUtils::StringToTimestamp(row[3].c_str());

            result.push_back(a);
        }
    } catch (const std::exception& ex) {
        Logger::Error("[CryptoRepository] Failed to get all crypto assets: {}", ex.what());
    }
    return result;
}

std::optional<CryptoPriceCandle> CryptoRepository::GetLastCryptoPrice(int asset_id) {
    try {
        auto conn_guard = db_.GetConnection();

        pqxx::nontransaction ntxn(conn_guard->Get());

        auto rows = ntxn.exec_params(
            "SELECT timestamp, open, high, low, close, volume "
            "FROM crypto_prices "
            "WHERE asset_id = $1 "
            "ORDER BY timestamp DESC "
            "LIMIT 1",
            asset_id
        );

        if (rows.empty()) {
            return std::nullopt;
        }

        auto row = rows[0];
        return CryptoPriceCandle{
            TimeUtils::StringToTimestamp(row[0].c_str()),
            row[1].as<double>(),
            row[2].as<double>(),
            row[3].as<double>(),
            row[4].as<double>(),
            row[5].as<double>()
        };
    } catch (const std::exception& ex) {
        Logger::Error("[CryptoRepository] Failed to get last crypto price for {}: {}", asset_id, ex.what());
        return std::nullopt;
    }
}

std::vector<CryptoPriceCandle> CryptoRepository::GetCryptoPricesHistory(int asset_id, 
                                                                            const Timestamp from, 
                                                                            const Timestamp to) {
    std::vector<CryptoPriceCandle> result;
    try {
        auto conn_guard = db_.GetConnection();

        pqxx::nontransaction ntxn(conn_guard->Get());

        std::string sql = R"(
            SELECT timestamp, open, high, low, close, volume 
            FROM crypto_prices 
            WHERE asset_id = $1 AND timestamp >= $2 AND timestamp <= $3 
            ORDER BY timestamp ASC
        )";

        auto rows = ntxn.exec_params(
                        sql, 
                        asset_id, 
                        TimeUtils::TimestampToString(from), 
                        TimeUtils::TimestampToString(to)
                    );

        result.reserve(rows.size());
        for (const auto& row : rows) {
            result.push_back({
                TimeUtils::StringToTimestamp(row[0].c_str()),
                row[1].as<double>(),
                row[2].as<double>(),
                row[3].as<double>(),
                row[4].as<double>(),
                row[5].as<double>()
            });
        }
    } catch (const std::exception& ex) {
        Logger::Error("[CryptoRepository] Failed to get crypto prices history for {}: {}", asset_id, ex.what());
    }
    return result;
}

void CryptoRepository::UpdateCryptoAssetName(int id, const std::string& new_name) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());

        txn.exec_params(
            "UPDATE crypto_assets "
            "SET name = $1 "
            "WHERE id = $2", 
            new_name, id
        );

        txn.commit();
        Logger::Debug("[CryptoRepository] Updated name for crypto_asset_id {}: {}", id, new_name);
    } catch (const std::exception& ex) {
        Logger::Error("[CryptoRepository] Failed to update name crypto_asset {}: {}", id, ex.what());
    }
}

void CryptoRepository::DeleteCryptoAsset(int id) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());

        txn.exec_params(
            "DELETE FROM crypto_assets "
            "WHERE id = $1", 
            id
        );

        txn.commit();
        Logger::Info("[CryptoRepository] Deleted crypto_asset: {}", id);
    } catch (const std::exception& ex) {
        Logger::Error("[CryptoRepository] Failed to delete crypto_asset {}: {}", id, ex.what());
    }
}

void CryptoRepository::DeleteOldCryptoPrices(int asset_id, const Timestamp older_than) {
    try {
        auto conn_guard = db_.GetConnection(); 

        pqxx::work txn(conn_guard->Get());

        txn.exec_params(
            "DELETE FROM crypto_prices "
            "WHERE asset_id = $1 AND timestamp < $2",
            asset_id, TimeUtils::TimestampToString(older_than)
        );
        
        txn.commit();
        Logger::Info("[CryptoRepository] Deleted crypto prices of asset_id {} older than {}", 
                        asset_id, TimeUtils::TimestampToString(older_than));
    } catch (const std::exception& ex) {
        Logger::Error("[CryptoRepository] Failed to delete crypto prices for {} older than {}: {}", 
                        asset_id, TimeUtils::TimestampToString(older_than), ex.what());
    }
}