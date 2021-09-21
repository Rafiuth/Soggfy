#pragma once
#include <string>
#include <unordered_map>
#include <cstdint>

namespace Http
{
    struct Request
    {
        std::string Url;

        std::unordered_map<std::string, std::string> Headers;
    };
    struct Response
    {
        //std::unordered_map<std::string, std::string> Headers;
        std::vector<uint8_t> Content;

        //Returns the content as an UTF8 string
        std::string AsString()
        {
            return std::string(Content.begin(), Content.end());
        }
    };

    //Performs an HTTP GET request
    Response Fetch(const Request& req);
}