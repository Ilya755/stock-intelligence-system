#include "CprHttpClient.hpp"

#include "cpr/cpr.h"

HttpResponse CprHttpClient::Get(const std::string& url, 
                                    const std::map<std::string, std::string>& params, 
                                    const std::map<std::string, std::string>& headers) {
    cpr::Parameters cpr_params;
    for (const auto& [key, value] : params) {
        cpr_params.Add({key, value});
    }
    cpr::Header cpr_headers;
    for (const auto& [key, value] : headers) {
        cpr_headers.insert({key, value});
    }

    cpr::Response r = cpr::Get(cpr::Url{url}, cpr_params, cpr_headers);

    HttpResponse resp;
    resp.status_code = static_cast<int>(r.status_code);
    if (r.error) {
        resp.error_message = r.error.message;
    } else {
        resp.text = r.text;
    }
    return resp;
}