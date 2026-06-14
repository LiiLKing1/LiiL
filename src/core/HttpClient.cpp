#include "HttpClient.h"
#include "Logger.h"
#include <windows.h>
#include <winhttp.h>
#include <memory>

#pragma comment(lib, "winhttp.lib")

namespace liil {
namespace core {

std::string HttpClient::Post(const std::string& url, const std::vector<std::string>& headers, const std::string& body) {
    std::string response;

    // Initialize WinHTTP
    HINTERNET hSession = WinHttpOpen(L"LiiL/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        Logger::Error("HttpClient: WinHttpOpen failed");
        return "";
    }

    // Convert URL to wide string
    int wlen = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, NULL, 0);
    std::wstring wUrl(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, &wUrl[0], wlen);

    // Crack the URL to extract host name and path
    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    
    wchar_t hostName[256];
    wchar_t urlPath[1024];
    
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = ARRAYSIZE(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = ARRAYSIZE(urlPath);
    urlComp.dwSchemeLength = (DWORD)-1; // Just needed to say we want scheme
    
    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) {
        Logger::Error("HttpClient: WinHttpCrackUrl failed");
        WinHttpCloseHandle(hSession);
        return "";
    }

    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) {
        Logger::Error("HttpClient: WinHttpConnect failed");
        WinHttpCloseHandle(hSession);
        return "";
    }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", urlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        Logger::Error("HttpClient: WinHttpOpenRequest failed");
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Add headers
    for (const auto& header : headers) {
        std::string h = header + "\r\n";
        int hlen = MultiByteToWideChar(CP_UTF8, 0, h.c_str(), -1, NULL, 0);
        std::wstring wh(hlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, h.c_str(), -1, &wh[0], hlen);
        
        WinHttpAddRequestHeaders(hRequest, wh.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    // Send Request
    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, 
                                       (LPVOID)body.c_str(), (DWORD)body.length(), (DWORD)body.length(), 0);

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    } else {
        Logger::Error("HttpClient: WinHttpSendRequest failed");
    }

    if (bResults) {
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                Logger::Error("HttpClient: WinHttpQueryDataAvailable failed");
                break;
            }
            if (dwSize == 0) break;
            
            std::unique_ptr<char[]> pszOutBuffer(new char[dwSize + 1]);
            ZeroMemory(pszOutBuffer.get(), dwSize + 1);
            
            if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer.get(), dwSize, &dwDownloaded)) {
                response.append(pszOutBuffer.get(), dwDownloaded);
            } else {
                Logger::Error("HttpClient: WinHttpReadData failed");
                break;
            }
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

std::string HttpClient::PostMultipart(const std::string& url, const std::vector<std::string>& headers, const std::string& filePath, const std::string& fileParamName, const std::string& modelName) {
    std::string response;

    // Faylni o'qish
    HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        Logger::Error("HttpClient: Multipart fayl ochilmadi: " + filePath);
        return "";
    }
    DWORD fileSize = GetFileSize(hFile, NULL);
    std::vector<char> fileBuffer(fileSize);
    DWORD bytesRead = 0;
    ReadFile(hFile, fileBuffer.data(), fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    // Boundary tayyorlash
    std::string boundary = "----WebKitFormBoundaryLiiLVoice123";
    
    std::string startBoundary = "--" + boundary + "\r\n";
    std::string endBoundary = "\r\n--" + boundary + "--\r\n";

    // 1-qism: Fayl
    std::string filePartHeader = startBoundary + 
        "Content-Disposition: form-data; name=\"" + fileParamName + "\"; filename=\"voice.wav\"\r\n" +
        "Content-Type: audio/wav\r\n\r\n";

    // 2-qism: Model parametri
    std::string modelPart = "\r\n" + startBoundary + 
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n" + modelName;

    // 3-qism: Language (Whisper API uchun faqat O'zbek tili)
    std::string langPart = "\r\n" + startBoundary + 
        "Content-Disposition: form-data; name=\"language\"\r\n\r\nuz";

    // Umumiy Buffer hajmini hisoblash
    DWORD totalSize = filePartHeader.size() + fileSize + modelPart.size() + langPart.size() + endBoundary.size();

    HINTERNET hSession = WinHttpOpen(L"LiiL/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    int wlen = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, NULL, 0);
    std::wstring wUrl(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, &wUrl[0], wlen);

    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostName[256];
    wchar_t urlPath[1024];
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = ARRAYSIZE(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = ARRAYSIZE(urlPath);
    urlComp.dwSchemeLength = (DWORD)-1; 
    
    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    HINTERNET hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", urlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    // Headerlar
    std::string contentTypeHeader = "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
    std::wstring wContentType(contentTypeHeader.begin(), contentTypeHeader.end());
    WinHttpAddRequestHeaders(hRequest, wContentType.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    for (const auto& header : headers) {
        std::string h = header + "\r\n";
        std::wstring wh(h.begin(), h.end());
        WinHttpAddRequestHeaders(hRequest, wh.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    // Yuborish (Binary chunk by chunk emas, bitta katta bufer qilsa ham bo'ladi xotirada, chunki ovoz uncha katta emas)
    std::vector<char> postBuffer;
    postBuffer.reserve(totalSize);
    postBuffer.insert(postBuffer.end(), filePartHeader.begin(), filePartHeader.end());
    postBuffer.insert(postBuffer.end(), fileBuffer.begin(), fileBuffer.end());
    postBuffer.insert(postBuffer.end(), modelPart.begin(), modelPart.end());
    postBuffer.insert(postBuffer.end(), langPart.begin(), langPart.end());
    postBuffer.insert(postBuffer.end(), endBoundary.begin(), endBoundary.end());

    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, 
                                       (LPVOID)postBuffer.data(), totalSize, totalSize, 0);

    if (bResults) bResults = WinHttpReceiveResponse(hRequest, NULL);

    if (bResults) {
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;
            
            std::unique_ptr<char[]> pszOutBuffer(new char[dwSize + 1]);
            ZeroMemory(pszOutBuffer.get(), dwSize + 1);
            
            if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer.get(), dwSize, &dwDownloaded)) {
                response.append(pszOutBuffer.get(), dwDownloaded);
            } else break;
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

} // namespace core
} // namespace liil
