#pragma once

#include <string>
#include <map>

struct HttpResponse {
    int status_code;
    std::string text;
    std::string error_message;
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    virtual HttpResponse Get(const std::string& url, 
                                const std::map<std::string, std::string>& params, 
                                const std::map<std::string, std::string>& headers = {}) = 0;
};