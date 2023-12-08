#pragma once
#include <string>
#include <vector>
#include <functional>

namespace CefUtils
{
    void InjectJS(const std::string& code);

    //Initializes the URL blocker hook.
    void InitUrlBlocker(std::function<bool(std::wstring_view)> isUrlBlocked);
}