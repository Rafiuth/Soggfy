#include "JsInjector.h"
#include "Hooks.h"

#include <memory>
#include <functional>

#include <type_traits>

//Compat stuff (these got removed in C++20)
namespace std
{
    template <class> struct result_of;
    template <class F, class... ArgTypes>
    struct result_of<F(ArgTypes...)> : std::invoke_result<void, F, ArgTypes...> {};
}

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

#pragma comment(lib, "../external/cef_bin/Release/libcef.lib")

//NOTE: libcef is compiled with clang, stl abi obviously differs from MS's
//Some APIs (specially those using STL features) won't work because of ABI incompatibility.

//llvm-pdbutil.exe pdb2yaml -tpi-stream ../cef_client_95.7.12+win32\libcef.dll.pdb > libcef_pdb.yaml
//was very useful for abi reference (only useable through a hex editor tho)

//Because of the API incompat: trying to free CefString returned by an API will result in a crash.
//This workaround will obviously leak memory, but it's small and rare enough that it isn't a big deal.
#define CEF_STRING_ABANDON(str) \
    str.clear();                \
    *(bool*)((void**)&str + 2) = false;      //url.owner_ = false

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
    //8B35 DCF13617            | mov esi,dword ptr ds:[1736F1DC]    //operand offset: 6
    //85F6                     | test esi,esi                   
    //74 44                    | je libcef.11D9BBA2             
    //803E 00                  | cmp byte ptr ds:[esi],0        
    //74 3F                    | je libcef.11D9BBA2             
    //807E 01 00               | cmp byte ptr ds:[esi+1],0      
    //75 39                    | jne libcef.11D9BBA2            
    //E8 82F0FFFF              | call libcef.11D9ABF0       
    Fingerprint fingerprint(
        L"libcef.dll",
        "\x55\x89\xE5\x56\x8B\x35\xDC\xF1\x36\x17\x85\xF6\x74\x44\x80\x3E\x00\x74\x3F\x80\x7E\x01\x00\x75\x39\xE8",
        "\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
    );
    uintptr_t addr = fingerprint.SearchInModule();
    return **(_CefContext***)(addr + 6);
}

namespace JsInjector
{
    void Inject(const std::string& code)
    {
        auto context = FindCefContext();

        for (auto& browser : context->GetBrowserList()) {
            auto mainFrame = browser->GetMainFrame();
            auto url = mainFrame->GetURL();
            mainFrame->ExecuteJavaScript(code, url, 0);

            CEF_STRING_ABANDON(url);
        }
    }
}