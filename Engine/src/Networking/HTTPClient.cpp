#include "Enjin/Platform/Platform.h"
#include "Enjin/Networking/HTTPClient.h"
#include "Enjin/Logging/Log.h"
#include <sstream>
#include <chrono>

#if ENJIN_PLATFORM_WEB
#include <emscripten/fetch.h>
#elif defined(_WIN32)
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace Enjin {
namespace Networking {

// ============================================================================
// URL Parsing
// ============================================================================

HTTPClient::ParsedURL HTTPClient::ParseURL(const std::string& url) {
    ParsedURL result;
    std::string remaining = url;

    // Protocol
    if (remaining.find("https://") == 0) {
        result.useSSL = true;
        result.port = 443;
        remaining = remaining.substr(8);
    } else if (remaining.find("http://") == 0) {
        result.useSSL = false;
        result.port = 80;
        remaining = remaining.substr(7);
    }

    // Host and path
    auto pathStart = remaining.find('/');
    if (pathStart != std::string::npos) {
        result.host = remaining.substr(0, pathStart);
        result.path = remaining.substr(pathStart);
    } else {
        result.host = remaining;
        result.path = "/";
    }

    // Port in host
    auto colonPos = result.host.find(':');
    if (colonPos != std::string::npos) {
        std::string portStr = result.host.substr(colonPos + 1);
        try {
            int p = std::stoi(portStr);
            if (p >= 1 && p <= 65535) {
                result.port = static_cast<u16>(p);
            } else {
                // NET-4: Log warning instead of silently falling back
                ENJIN_LOG_WARN(Network, "HTTPClient: Port %d out of range in URL, defaulting to %u", p, result.port);
            }
        } catch (...) {
            // NET-4: Log warning on unparseable port instead of silent fallback
            ENJIN_LOG_WARN(Network, "HTTPClient: Invalid port '%s' in URL, defaulting to %u", portStr.c_str(), result.port);
        }
        result.host = result.host.substr(0, colonPos);
    }

    return result;
}

// ============================================================================
// Windows Implementation (WinHTTP)
// ============================================================================

#ifdef _WIN32

static std::wstring ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size);
    return result;
}

static HTTPResponse DoRequest(const std::string& method, const std::string& url,
                               const std::string& body,
                               const std::unordered_map<std::string, std::string>& headers) {
    HTTPResponse response;
    auto parsed = HTTPClient::ParseURL(url);

    // Open session
    HINTERNET hSession = WinHttpOpen(L"EnjinEngine/1.0",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        response.error = "WinHttpOpen failed";
        return response;
    }

    // Connect
    std::wstring wHost = ToWide(parsed.host);
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), parsed.port, 0);
    if (!hConnect) {
        response.error = "WinHttpConnect failed";
        WinHttpCloseHandle(hSession);
        return response;
    }

    // Open request
    std::wstring wPath = ToWide(parsed.path);
    std::wstring wMethod = ToWide(method);
    DWORD flags = parsed.useSSL ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), wPath.c_str(),
                                             nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        response.error = "WinHttpOpenRequest failed";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
    }

    // Set timeout (10 seconds)
    WinHttpSetTimeouts(hRequest, 10000, 10000, 10000, 10000);

    // Add custom headers
    for (auto& [key, value] : headers) {
        std::wstring header = ToWide(key + ": " + value);
        WinHttpAddRequestHeaders(hRequest, header.c_str(), (DWORD)header.size(),
                                  WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    // Add content-type for POST
    if (method == "POST" && !body.empty() &&
        headers.find("Content-Type") == headers.end()) {
        WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json",
                                  (DWORD)-1L,
                                  WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    // Send request
    LPVOID bodyData = body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.c_str();
    DWORD bodyLen = body.empty() ? 0 : (DWORD)body.size();
    BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    bodyData, bodyLen, bodyLen, 0);
    if (!sent) {
        response.error = "WinHttpSendRequest failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        response.error = "WinHttpReceiveResponse failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
    }

    // Status code
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
                         WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                         WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
                         WINHTTP_NO_HEADER_INDEX);
    response.statusCode = (i32)statusCode;

    // Read body (N14: cap at 16MB to prevent OOM from malicious server)
    static constexpr size_t MAX_RESPONSE_SIZE = 16 * 1024 * 1024;
    std::string responseBody;
    DWORD bytesAvailable = 0;
    do {
        bytesAvailable = 0;
        WinHttpQueryDataAvailable(hRequest, &bytesAvailable);
        if (bytesAvailable == 0) break;

        std::vector<char> buffer(bytesAvailable + 1, 0);
        DWORD bytesRead = 0;
        WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead);
        responseBody.append(buffer.data(), bytesRead);
        if (responseBody.size() > MAX_RESPONSE_SIZE) {
            ENJIN_LOG_WARN(Network, "HTTP response exceeds 16MB limit, truncating");
            break;
        }
    } while (bytesAvailable > 0);

    response.body = responseBody;
    response.success = (statusCode >= 200 && statusCode < 300);

    // S-M9: When response was truncated at 16MB, mark as truncated and warn
    if (responseBody.size() >= MAX_RESPONSE_SIZE) {
        response.success = false;
        response.error = "Response truncated at 16MB limit";
        ENJIN_LOG_WARN(Network, "HTTP response truncated at 16MB, marking as unsuccessful");
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

HTTPResponse HTTPClient::Get(const std::string& url,
                              const std::unordered_map<std::string, std::string>& headers) {
    return DoRequest("GET", url, "", headers);
}

HTTPResponse HTTPClient::Post(const std::string& url,
                               const std::string& body,
                               const std::unordered_map<std::string, std::string>& headers) {
    return DoRequest("POST", url, body, headers);
}

// S6: RFC 3986 percent-encoding for form POST key/value pairs
static std::string PercentEncode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

HTTPResponse HTTPClient::PostForm(const std::string& url,
                                   const std::unordered_map<std::string, std::string>& params,
                                   const std::unordered_map<std::string, std::string>& headers) {
    // Build URL-encoded form body — S6: properly percent-encode keys and values
    std::string body;
    for (auto& [key, value] : params) {
        if (!body.empty()) body += "&";
        body += PercentEncode(key) + "=" + PercentEncode(value);
    }

    auto h = headers;
    h["Content-Type"] = "application/x-www-form-urlencoded";
    return DoRequest("POST", url, body, h);
}

HTTPResponse HTTPClient::PostMultipart(
    const std::string& url,
    const std::unordered_map<std::string, std::string>& fields,
    const std::vector<MultipartFile>& files,
    const std::unordered_map<std::string, std::string>& headers)
{
    // Build multipart/form-data body manually
    std::string boundary = "----EnjinBoundary" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());

    std::string body;
    // Text fields
    for (auto& [key, value] : fields) {
        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"" + key + "\"\r\n\r\n";
        body += value + "\r\n";
    }
    // File fields
    for (auto& f : files) {
        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"" + f.fieldName +
                "\"; filename=\"" + f.fileName + "\"\r\n";
        body += "Content-Type: " + f.contentType + "\r\n\r\n";
        body.append(reinterpret_cast<const char*>(f.data.data()), f.data.size());
        body += "\r\n";
    }
    body += "--" + boundary + "--\r\n";

    auto h = headers;
    h["Content-Type"] = "multipart/form-data; boundary=" + boundary;
    return DoRequest("POST", url, body, h);
}

#elif defined(ENJIN_HAS_CURL)

// ============================================================================
// libcurl Implementation (Linux / macOS)
// ============================================================================

#include <curl/curl.h>

static size_t CurlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userData) {
    auto* response = static_cast<std::string*>(userData);
    size_t bytes = size * nmemb;
    static constexpr size_t MAX_RESPONSE_SIZE = 16 * 1024 * 1024;
    if (response->size() + bytes > MAX_RESPONSE_SIZE) {
        ENJIN_LOG_WARN(Network, "HTTP response exceeds 16MB limit, truncating");
        return 0; // Abort transfer
    }
    response->append(ptr, bytes);
    return bytes;
}

static HTTPResponse DoRequestCurl(const std::string& method, const std::string& url,
                                   const std::string& body,
                                   const std::unordered_map<std::string, std::string>& headers) {
    HTTPResponse response;

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = "curl_easy_init failed";
        return response;
    }

    std::string responseBody;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "EnjinEngine/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Set method
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    }

    // Build header list
    struct curl_slist* headerList = nullptr;
    for (auto& [key, value] : headers) {
        std::string h = key + ": " + value;
        headerList = curl_slist_append(headerList, h.c_str());
    }
    // Default Content-Type for POST
    if (method == "POST" && !body.empty() &&
        headers.find("Content-Type") == headers.end()) {
        headerList = curl_slist_append(headerList, "Content-Type: application/json");
    }
    if (headerList) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        response.error = std::string("curl error: ") + curl_easy_strerror(res);
    } else {
        long statusCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
        response.statusCode = static_cast<i32>(statusCode);
        response.body = std::move(responseBody);
        response.success = (statusCode >= 200 && statusCode < 300);
    }

    if (headerList) curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
    return response;
}

// S6: RFC 3986 percent-encoding for form POST key/value pairs
static std::string PercentEncodeCurl(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

HTTPResponse HTTPClient::Get(const std::string& url,
                              const std::unordered_map<std::string, std::string>& headers) {
    return DoRequestCurl("GET", url, "", headers);
}

HTTPResponse HTTPClient::Post(const std::string& url,
                               const std::string& body,
                               const std::unordered_map<std::string, std::string>& headers) {
    return DoRequestCurl("POST", url, body, headers);
}

HTTPResponse HTTPClient::PostForm(const std::string& url,
                                   const std::unordered_map<std::string, std::string>& params,
                                   const std::unordered_map<std::string, std::string>& headers) {
    std::string body;
    for (auto& [key, value] : params) {
        if (!body.empty()) body += "&";
        body += PercentEncodeCurl(key) + "=" + PercentEncodeCurl(value);
    }
    auto h = headers;
    h["Content-Type"] = "application/x-www-form-urlencoded";
    return DoRequestCurl("POST", url, body, h);
}

HTTPResponse HTTPClient::PostMultipart(
    const std::string& url,
    const std::unordered_map<std::string, std::string>& fields,
    const std::vector<MultipartFile>& files,
    const std::unordered_map<std::string, std::string>& headers)
{
    // Build multipart/form-data body manually
    std::string boundary = "----EnjinBoundary" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());

    std::string body;
    for (auto& [key, value] : fields) {
        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"" + key + "\"\r\n\r\n";
        body += value + "\r\n";
    }
    for (auto& f : files) {
        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"" + f.fieldName +
                "\"; filename=\"" + f.fileName + "\"\r\n";
        body += "Content-Type: " + f.contentType + "\r\n\r\n";
        body.append(reinterpret_cast<const char*>(f.data.data()), f.data.size());
        body += "\r\n";
    }
    body += "--" + boundary + "--\r\n";

    auto h = headers;
    h["Content-Type"] = "multipart/form-data; boundary=" + boundary;
    return DoRequestCurl("POST", url, body, h);
}

#else

// ============================================================================
// Stub Implementation (No HTTP backend available)
// ============================================================================

HTTPResponse HTTPClient::Get(const std::string& url,
                              const std::unordered_map<std::string, std::string>& headers) {
    (void)url; (void)headers;
    return { false, 0, "", {}, "HTTP not available on this platform" };
}

HTTPResponse HTTPClient::Post(const std::string& url,
                               const std::string& body,
                               const std::unordered_map<std::string, std::string>& headers) {
    (void)url; (void)body; (void)headers;
    return { false, 0, "", {}, "HTTP not available on this platform" };
}

HTTPResponse HTTPClient::PostForm(const std::string& url,
                                   const std::unordered_map<std::string, std::string>& params,
                                   const std::unordered_map<std::string, std::string>& headers) {
    (void)url; (void)params; (void)headers;
    return { false, 0, "", {}, "HTTP not available on this platform" };
}

HTTPResponse HTTPClient::PostMultipart(
    const std::string& url,
    const std::unordered_map<std::string, std::string>& fields,
    const std::vector<MultipartFile>& files,
    const std::unordered_map<std::string, std::string>& headers) {
    (void)url; (void)fields; (void)files; (void)headers;
    return { false, 0, "", {}, "HTTP not available on this platform" };
}

#endif // _WIN32 / ENJIN_HAS_CURL

} // namespace Networking
} // namespace Enjin
