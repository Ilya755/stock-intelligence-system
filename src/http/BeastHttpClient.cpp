#include "BeastHttpClient.hpp"

#include <cctype>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "boost/asio/connect.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/redirect_error.hpp"
#include "boost/asio/ssl/host_name_verification.hpp"
#include "boost/asio/ssl/stream.hpp"
#include "boost/asio/use_awaitable.hpp"
#include "boost/beast/core.hpp"
#include "boost/beast/http.hpp"
#include "boost/beast/ssl.hpp"
#include "openssl/ssl.h"

namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

struct ParsedUrl {
    bool use_tls;
    std::string host;
    std::string port;
    std::string target;
};

ParsedUrl ParseUrl(const std::string& url) {
    const auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        throw std::invalid_argument("URL is missing a scheme: " + url);
    }

    const auto scheme = url.substr(0, scheme_end);
    const bool use_tls = scheme == "https";
    if (!use_tls && scheme != "http") {
        throw std::invalid_argument("Unsupported URL scheme: " + scheme);
    }

    const auto authority_start = scheme_end + 3;
    const auto path_start = url.find('/', authority_start);
    const auto authority = url.substr(authority_start, path_start - authority_start);
    if (authority.empty()) {
        throw std::invalid_argument("URL is missing a host: " + url);
    }

    const auto colon = authority.rfind(':');
    const bool has_port = colon != std::string::npos && authority.find(']') == std::string::npos;

    ParsedUrl parsed;
    parsed.use_tls = use_tls;
    parsed.host = has_port ? authority.substr(0, colon) : authority;
    parsed.port = has_port ? authority.substr(colon + 1) : (use_tls ? "443" : "80");
    parsed.target = path_start == std::string::npos ? "/" : url.substr(path_start);

    return parsed;
}

std::string UrlEncode(const std::string& value) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;
    for (const unsigned char ch : value) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            encoded << ch;
        } else {
            encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        }
    }
    return encoded.str();
}

std::string BuildTarget(std::string target, const std::map<std::string, std::string>& params) {
    char separator = target.find('?') == std::string::npos ? '?' : '&';
    for (const auto& [key, value] : params) {
        target += separator;
        separator = '&';
        target += UrlEncode(key) + "=" + UrlEncode(value);
    }
    return target;
}

http::request<http::empty_body> BuildRequest(
        const ParsedUrl& url,
        const std::map<std::string, std::string>& params,
        const std::map<std::string, std::string>& headers) {
    http::request<http::empty_body> request{http::verb::get, BuildTarget(url.target, params), 11};
    request.set(http::field::host, url.host);
    request.set(http::field::user_agent, "stock-intelligence-system/1.0");
    for (const auto& [key, value] : headers) {
        request.set(key, value);
    }
    return request;
}

HttpResponse ToHttpResponse(const http::response<http::string_body>& response) {
    return HttpResponse{
        .status_code = static_cast<int>(response.result_int()),
        .text = response.body(),
        .error_message = response.result() == http::status::ok
            ? std::string{}
            : std::string(response.reason().data(), response.reason().size())
    };
}

}  

BeastHttpClient::BeastHttpClient(boost::asio::any_io_executor executor)
    : executor_(std::move(executor))
    , ssl_context_(boost::asio::ssl::context::tls_client) {
        ssl_context_.set_default_verify_paths();
        ssl_context_.set_verify_mode(boost::asio::ssl::verify_peer);
    }

boost::asio::awaitable<HttpResponse> BeastHttpClient::GetAsync(
        const std::string& url, 
        const std::map<std::string, std::string>& params, 
        const std::map<std::string, std::string>& headers) {
    try {
        const auto parsed = ParseUrl(url);
        auto request = BuildRequest(parsed, params, headers);
        tcp::resolver resolver(executor_);
        auto endpoints = co_await resolver.async_resolve(parsed.host, parsed.port, asio::use_awaitable);

        beast::flat_buffer buffer;
        http::response<http::string_body> response;

        if (parsed.use_tls) {
            beast::ssl_stream<beast::tcp_stream> stream(executor_, ssl_context_);
            if (!SSL_set_tlsext_host_name(stream.native_handle(), parsed.host.c_str())) {
                throw boost::system::system_error(
                    static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category());
            }
            stream.set_verify_callback(asio::ssl::host_name_verification(parsed.host));

            beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(30));
            co_await beast::get_lowest_layer(stream).async_connect(endpoints, asio::use_awaitable);
            co_await stream.async_handshake(asio::ssl::stream_base::client, asio::use_awaitable);
            co_await http::async_write(stream, request, asio::use_awaitable);
            co_await http::async_read(stream, buffer, response, asio::use_awaitable);

            boost::system::error_code shutdown_error;
            co_await stream.async_shutdown(asio::redirect_error(asio::use_awaitable, shutdown_error));
            if (shutdown_error == asio::error::eof || shutdown_error == asio::ssl::error::stream_truncated) {
                shutdown_error = {};
            }
            if (shutdown_error) {
                throw boost::system::system_error(shutdown_error);
            }
        } else {
            beast::tcp_stream stream(executor_);
            stream.expires_after(std::chrono::seconds(30));
            co_await stream.async_connect(endpoints, asio::use_awaitable);
            co_await http::async_write(stream, request, asio::use_awaitable);
            co_await http::async_read(stream, buffer, response, asio::use_awaitable);
            stream.socket().shutdown(tcp::socket::shutdown_both);
        }

        co_return ToHttpResponse(response);
    } catch (const std::exception& ex) {
        co_return HttpResponse{
            .status_code = 0,
            .text = {},
            .error_message = ex.what()
        };
    }
}