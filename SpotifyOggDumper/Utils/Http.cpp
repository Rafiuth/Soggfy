#include "../pch.h"
#include "Http.h"
#include <winhttp.h>

namespace Http
{
    //adapted from https://datatracker.ietf.org/doc/html/rfc3986#page-50
    //[1] scheme
    //[3] host
    //[4] path and args
    std::regex g_UrlRegex(R"(^(https?:)?(\/\/([^\/?#]*))?(([^?#]*)(\?([^#]*))?(#(.*))?))");

    Response Fetch(const Request& reqData)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> utfConv;

        std::smatch url;
        if (!std::regex_match(reqData.Url, url, g_UrlRegex)) {
            throw std::exception("Failed to parse request url");
        }
        auto scheme = url.str(1);
        bool isHttps = _stricmp("https", scheme.c_str());
        auto hostW = utfConv.from_bytes(url.str(3));
        auto pathW = utfConv.from_bytes(url.str(4));
        auto headersW = std::wstring();

        for (auto& hdr : reqData.Headers) {
            headersW += utfConv.from_bytes(hdr.first + ": " + hdr.second + "\r\n");
        }

        BOOL bResults = FALSE;
        HINTERNET hSession = NULL,
            hConnect = NULL,
            hRequest = NULL;

        // Use WinHttpOpen to obtain a session handle.
        hSession = WinHttpOpen(NULL,
                               WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                               WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0);

        // Specify an HTTP server.
        if (hSession) {
            DWORD optVal = WINHTTP_DECOMPRESSION_FLAG_ALL;
            WinHttpSetOption(hSession, WINHTTP_OPTION_DECOMPRESSION, &optVal, 4);

            optVal = WINHTTP_PROTOCOL_FLAG_HTTP2;
            WinHttpSetOption(hSession, WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &optVal, 4);

            hConnect = WinHttpConnect(hSession, hostW.c_str(), isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
        }

        // Create an HTTP request handle.
        if (hConnect)
            hRequest = WinHttpOpenRequest(hConnect, L"GET", pathW.c_str(),
                                          NULL, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES,
                                          isHttps ? WINHTTP_FLAG_SECURE : 0);

        if (hRequest)
            WinHttpAddRequestHeaders(hRequest, headersW.c_str(), headersW.size(), 0);

        // Send a request.
        if (hRequest)
            bResults = WinHttpSendRequest(hRequest,
                                           WINHTTP_NO_ADDITIONAL_HEADERS,
                                           0, WINHTTP_NO_REQUEST_DATA, 0,
                                           0, 0);


        // End the request.
        if (bResults)
            bResults = WinHttpReceiveResponse(hRequest, NULL);

        Response respData;

        // Continue to verify data until there is nothing left.
        if (bResults) {
            while (true) {
                DWORD bytesAvail = 0, bytesRead = 0;
                bResults = WinHttpQueryDataAvailable(hRequest, &bytesAvail);
                if (!bResults || bytesAvail == 0) break;

                auto& content = respData.Content;
                int prevSize = content.size();

                //try to alloc 2x the current buffer size to avoid reallocating every chunk
                if (content.capacity() < prevSize + bytesAvail) {
                    content.reserve(max(prevSize * 2, bytesAvail));
                }
                content.resize(prevSize + bytesAvail);
                
                bResults = WinHttpReadData(hRequest, content.data() + prevSize, bytesAvail, &bytesRead);
                content.resize(prevSize + bytesRead);
            }
        }
        int errorCode = !bResults ? GetLastError() : 0;

        // Close open handles.
        if (hRequest) WinHttpCloseHandle(hRequest);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);

        // Report any errors.
        if (!bResults) {
            throw std::runtime_error("Failed to perform http request (error " + std::to_string(errorCode) + ")");
        }
        return respData;
    }
}