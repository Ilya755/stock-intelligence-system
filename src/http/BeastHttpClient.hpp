#pragma once

#include "boost/asio/any_io_executor.hpp"
#include "boost/asio/ssl/context.hpp"

#include "IHttpClient.hpp"

class BeastHttpClient : public IHttpClient {
public:
    explicit BeastHttpClient(boost::asio::any_io_executor executor);

    boost::asio::awaitable<HttpResponse> GetAsync(
        const std::string& url, 
        const std::map<std::string, std::string>& params, 
        const std::map<std::string, std::string>& headers = {}) override;

private:
    boost::asio::any_io_executor executor_;
    boost::asio::ssl::context ssl_context_;
};