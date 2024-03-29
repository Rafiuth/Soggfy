#include "CefUtils.h"
#include "Utils/Hooks.h"
#include "Utils/Log.h"

#include <memory>

#include <type_traits>

//hack to prevent linking to cef library
#ifndef NDEBUG
#define NDEBUG
#endif

#include <include/base/cef_logging.h>
#undef DCHECK
#define DCHECK

#include <include/cef_app.h>
#include <include/cef_browser.h>
#include <include/base/cef_weak_ptr.h>
#include <include/capi/cef_thread_capi.h>
#include <include/internal/cef_types_wrappers.h>
#include <include/cef_version.h>
#include <include/capi/cef_urlrequest_capi.h>

#pragma comment(lib, "../external/cef_bin/Release/libcef.lib")

//NOTE: libcef is compiled with clang, stl abi obviously differs from MS's
//Some APIs (specially those using STL features) won't work because of ABI incompatibility.

//llvm-pdbutil.exe pdb2yaml -tpi-stream ../cef_client_95.7.12+win32\libcef.dll.pdb > libcef_pdb.yaml
//was very useful for abi reference (only useable through a hex editor tho)

//Because of the API incompat: trying to free CefString returned by an API will result in a crash.
//This workaround will obviously leak memory, but it's small and rare enough that it isn't a big deal.
#define CEF_STRING_ABANDON(str)     url.Detach()

struct _CefBrowserInfo;
typedef void* _CefLock;  //ABI: pthreads=size_t(4/8) vs msvc=CRITICAL_SECTION(24/40)

//https://github.com/llvm-mirror/libcxx/blob/master/include/list
//https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/include/bits/stl_list.h (slightly more readable)
//typedef std::list<scoped_refptr<_CefBrowserInfo>> BrowserInfoList;
template<class T>
struct _libcxx_list
{
    struct Node
    {
        Node* next_;
        Node* prev_;
        T value_;
    };
    Node root_;

    size_t size()
    {
        return (size_t)root_.value_;
    }
    void Traverse(std::function<void(T&)> visitor)
    {
        Node* node = root_.next_;
        while (node != &root_) {
            visitor(node->value_);
            node = node->next_;
        }
    }
};

class _CefBrowserHostBase :
    public CefBrowserHost,
    public CefBrowser
{
    virtual void InitializeBrowser() = 0;
};

struct _CefBrowserInfo
{
    void* _vtable;
    base::AtomicRefCount _ref_count;

    int browser_id_;
    bool is_popup_;
    bool is_windowless_;

    CefBrowser* GetBrowser()
    {
        int ver = cef_version_info(6); //chrome build

        auto ptr = (char*)this;
        if (ver >= 4515) {
            //https://bitbucket.org/chromiumembedded/cef/commits/e411b513beb705f8298bd60b2110da20abecae34
            ptr += 44;
        } else {
            ptr += 32;
        }
        auto browserHost = (CefRefPtr<_CefBrowserHostBase>*)ptr;
        return dynamic_cast<CefBrowser*>(browserHost->get());
    }
};

struct _CefBrowserInfoManager
{
    void* _vtable;
    mutable base::WeakPtrFactory<void> factory_;

    mutable _CefLock browser_info_lock_;
    _libcxx_list<_CefBrowserInfo*> browser_info_list_;
    int next_browser_id_ = 0;
};

//https://bitbucket.org/chromiumembedded/cef/src/master/libcef/browser/context.h?at=master#context.h-101
class _CefContext
{
public:
    bool initialized_;
    bool shutting_down_;

    // The thread on which the context was initialized.
    cef_platform_thread_id_t init_thread_id_;

    CefSettings settings_;
    //CefRefPtr<CefApp> application_;
    //
    //std::unique_ptr<void> main_runner_;
    //std::unique_ptr<void> trace_subscriber_;
    //std::unique_ptr<_CefBrowserInfoManager> browser_info_manager_;

    _CefBrowserInfoManager* GetBrowserInfoManager()
    {
        auto ptr = (char*)&settings_;
        ptr += sizeof(CefSettings) - sizeof(cef_settings_t) + settings_.size;
        ptr += sizeof(intptr_t) * 3;

        return *(_CefBrowserInfoManager**)ptr;
    }

    std::vector<CefBrowser*> GetBrowserList()
    {
        auto infoMgr = GetBrowserInfoManager();
        auto list = infoMgr->browser_info_list_;

        std::vector<CefBrowser*> vec;
        infoMgr->browser_info_list_.Traverse([&](auto& b) {
            vec.push_back(b->GetBrowser());
        });
        return vec;
    }
};

_CefContext* FindCefContext()
{
    //CefShutdown
    //55                       | push ebp                       
    //89E5                     | mov ebp,esp                    
    //56                       | push esi                       
    //8B35 0C792073            | mov esi,dword ptr ds:[7320790C]    ; at +6 bytes
    //85F6                     | test esi,esi                   
    //74 0B                    | je libcef.69175F79             
    //803E 00                  | cmp byte ptr ds:[esi],0        
    //74 06                    | je libcef.69175F79             
    //807E 01 00               | cmp byte ptr ds:[esi+1],0      
    //74 03                    | je libcef.69175F7C             
    Hooks::DataPattern pattern("55 89 E5 56 8B 35 ?? ?? ?? ?? 85 F6 74 ?? 80 3E 00 74 ?? 80 7E 01 00 7?");

    const uint8_t* codeBase;
    size_t codeLength;
    Hooks::GetModuleCode("libcef.dll", &codeBase, &codeLength);

    int32_t offset = pattern.FindNext(codeBase, codeLength);
    if (offset < 0) {
        LogError("Could not find cef_shutdown()");
        throw std::exception("Could not find CefContext");
    }
    return **(_CefContext***)(codeBase + offset + 6);
}

namespace CefUtils
{
    void InjectJS(const std::string& code)
    {
        auto context = FindCefContext();

        for (auto& browser : context->GetBrowserList()) {
            auto mainFrame = browser->GetMainFrame();
            auto url = mainFrame->GetURL();
            mainFrame->ExecuteJavaScript(code, url, 0);

            CEF_STRING_ABANDON(url);
        }
    }

    std::function<bool(std::wstring_view)> _isUrlBlocked;

    DETOUR_FUNC(__cdecl, cef_urlrequest_t*, UrlRequestCreate, (
        _cef_request_t* request,
        _cef_urlrequest_client_t* client,
        _cef_request_context_t* request_context
    ))
    {
        auto url = request->get_url(request);
        auto urlsv = std::wstring_view((wchar_t*)url->str, url->length);
        bool blocked = _isUrlBlocked(urlsv);

        if (LogMinLevel <= LOG_TRACE) {
            LogTrace("CefRequestCreate: [{}] {}", blocked ? "x" : ">", std::string(urlsv.begin(), urlsv.end()));
        }
        cef_string_userfree_free(url);
        return blocked ? nullptr : UrlRequestCreate_Orig(request, client, request_context);
    }

    void InitUrlBlocker(std::function<bool(std::wstring_view)> isUrlBlocked)
    {
        _isUrlBlocked = isUrlBlocked;
        Hooks::CreateApi(L"libcef.dll", "cef_urlrequest_create", &UrlRequestCreate_Detour, (Hooks::FuncAddr*)&UrlRequestCreate_Orig);
    }
}