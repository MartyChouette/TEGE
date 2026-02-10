#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <unordered_map>

namespace Enjin {
namespace Networking {

// HTTP response data
struct HTTPResponse {
    bool success = false;
    i32 statusCode = 0;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    std::string error;
};

// Simple HTTP client (WinHTTP on Windows, stub on other platforms)
class ENJIN_API HTTPClient {
public:
    // GET request
    static HTTPResponse Get(const std::string& url,
                            const std::unordered_map<std::string, std::string>& headers = {});

    // POST request with JSON body
    static HTTPResponse Post(const std::string& url,
                             const std::string& body,
                             const std::unordered_map<std::string, std::string>& headers = {});

    // POST request with form data
    static HTTPResponse PostForm(const std::string& url,
                                 const std::unordered_map<std::string, std::string>& params,
                                 const std::unordered_map<std::string, std::string>& headers = {});

    // URL parsing helper
    struct ParsedURL {
        std::string host;
        std::string path;
        u16 port = 443;
        bool useSSL = true;
    };
    static ParsedURL ParseURL(const std::string& url);
};

} // namespace Networking
} // namespace Enjin
