#pragma once
#include <string>
#include <vector>
#include <functional>

namespace CefUtils
{
    void InjectJS(const std::string& code);

    typedef void* OpaqueFn;
    //Initializes the URL blocker and returns a pair of <Detour, OrigFunc> for cef_urlrequest_create() hook.
    std::pair<OpaqueFn, OpaqueFn*> InitUrlBlocker(std::function<bool(std::wstring_view)> isUrlBlocked);
}