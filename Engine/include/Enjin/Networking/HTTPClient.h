#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <unordered_map>
#include <vector>

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

    // Multipart form-data file for PostMultipart
    struct MultipartFile {
        std::string fieldName;   // e.g. "file"
        std::string fileName;    // e.g. "screenshot.png"
        std::string contentType; // e.g. "image/png"
        std::vector<u8> data;    // Raw file bytes
    };

    // POST request with multipart/form-data (text fields + file attachments)
    // Used for Discord webhook file uploads and similar APIs.
    static HTTPResponse PostMultipart(
        const std::string& url,
        const std::unordered_map<std::string, std::string>& fields,
        const std::vector<MultipartFile>& files,
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
