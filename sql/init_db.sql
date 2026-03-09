CREATE TYPE report_type_enum AS ENUM ('annual', 'quarterly', 'ttm');

CREATE TABLE IF NOT EXISTS companies (
    id SERIAL PRIMARY KEY,
    ticker VARCHAR(10) NOT NULL,
    name VARCHAR(255) NOT NULL,
    country VARCHAR(50),
    sector VARCHAR(100),
    industry VARCHAR(100),
    currency VARCHAR(10) NOT NULL DEFAULT 'USD',
    description TEXT,
    exchange VARCHAR(50) NOT NULL, 
    updated_at TIMESTAMP NOT NULL DEFAULT NOW(),

    UNIQUE(ticker, exchange)
);

CREATE TABLE IF NOT EXISTS companies_financial_reports (
    id SERIAL PRIMARY KEY,
    company_id INT REFERENCES companies(id) ON DELETE CASCADE,
    period_date DATE NOT NULL,
    report_type report_type_enum NOT NULL,
    currency VARCHAR(10) NOT NULL DEFAULT 'USD',
    revenue DECIMAL(20, 2) NOT NULL,
    net_income DECIMAL(20, 2) NOT NULL,
    total_debt DECIMAL(20, 2) NOT NULL,
    equity DECIMAL(20, 2) NOT NULL,

    UNIQUE(company_id, period_date, report_type)
);

CREATE TABLE IF NOT EXISTS stock_dividends (
    id SERIAL PRIMARY KEY,
    company_id INT REFERENCES companies(id) ON DELETE CASCADE,
    ex_date DATE NOT NULL,         
    payment_date DATE,         
    amount DECIMAL(10, 4) NOT NULL, 
    
    UNIQUE(company_id, ex_date)
);

CREATE TABLE IF NOT EXISTS stock_prices (
    company_id INT REFERENCES companies(id) ON DELETE CASCADE,
    timestamp TIMESTAMP NOT NULL,
    open DECIMAL(15, 4) NOT NULL,
    high DECIMAL(15, 4) NOT NULL,
    low DECIMAL(15, 4) NOT NULL,
    close DECIMAL(15, 4) NOT NULL,
    volume BIGINT NOT NULL,                 
    
    PRIMARY KEY (company_id, timestamp)
);

CREATE TABLE IF NOT EXISTS stock_splits (
    id SERIAL PRIMARY KEY,
    company_id INT REFERENCES companies(id) ON DELETE CASCADE,
    split_date DATE NOT NULL,
    numerator DECIMAL(10, 4) NOT NULL, 
    denominator DECIMAL(10, 4) NOT NULL, 
    
    UNIQUE(company_id, split_date)
);

CREATE TABLE crypto_assets (
    id SERIAL PRIMARY KEY,
    ticker VARCHAR(10) NOT NULL UNIQUE,
    name VARCHAR(100) NOT NULL,
    updated_at TIMESTAMP NOT NULL DEFAULT NOW() 
);

CREATE TABLE crypto_prices (
    asset_id INT REFERENCES crypto_assets(id) ON DELETE CASCADE,
    timestamp TIMESTAMP NOT NULL,
    open DECIMAL(20, 8) NOT NULL,
    high DECIMAL(20, 8) NOT NULL,
    low DECIMAL(20, 8) NOT NULL,
    close DECIMAL(20, 8) NOT NULL,
    volume DECIMAL(20, 2) NOT NULL,
    
    PRIMARY KEY (asset_id, timestamp)
);

CREATE TABLE IF NOT EXISTS market_news (
    id SERIAL PRIMARY KEY,
    ticker VARCHAR(20), 
    headline TEXT NOT NULL,
    source VARCHAR(100),
    url TEXT UNIQUE,
    summary TEXT,
    published_at TIMESTAMP NOT NULL
);

CREATE TABLE IF NOT EXISTS data_update_logs (
    company_id INT NOT NULL,
    data_type VARCHAR(50) NOT NULL, 
    updated_at TIMESTAMP NOT NULL DEFAULT NOW(),
    
    PRIMARY KEY (company_id, data_type),
    CONSTRAINT fk_company_log FOREIGN KEY(company_id) 
        REFERENCES companies(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_stock_prices_time ON stock_prices(timestamp);
CREATE INDEX IF NOT EXISTS idx_crypto_prices_time ON crypto_prices(timestamp);
CREATE INDEX IF NOT EXISTS idx_news_time ON market_news(published_at);
