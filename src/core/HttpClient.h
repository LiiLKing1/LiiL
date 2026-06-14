#pragma once
#include <string>
#include <vector>

namespace liil {
namespace core {

class HttpClient {
public:
    // Performs a synchronous HTTP POST request using WinHTTP
    // url: full URL (e.g. https://openrouter.ai/api/v1/chat/completions)
    // headers: list of raw header strings (e.g. "Authorization: Bearer <key>")
    // body: JSON payload
    // Returns: The response body as a string, or empty string on failure.
    static std::string Post(const std::string& url, 
                            const std::vector<std::string>& headers, 
                            const std::string& body);

    // Uploads a file using multipart/form-data
    static std::string PostMultipart(const std::string& url,
                                     const std::vector<std::string>& headers,
                                     const std::string& filePath,
                                     const std::string& fileParamName,
                                     const std::string& modelName);
};

} // namespace core
} // namespace liil
