#pragma once

#include "IHttpClient.hpp"

class CprHttpClient : public IHttpClient {
public:
    HttpResponse Get(const std::string& url, 
                        const std::map<std::string, std::string>& params, 
                        const std::map<std::string, std::string>& headers = {}) override;
};