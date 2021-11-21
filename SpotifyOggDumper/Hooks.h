#pragma once
#include "pch.h"

struct Fingerprint
{
    const wchar_t* ModuleName;
    const char* Pattern;
    const char* Mask;
    int Length;

    template<size_t len>
    Fingerprint(const wchar_t* modName, const char(&pattern)[len], const char(&mask)[len])
    {
        ModuleName = modName;
        Pattern = pattern;
        Mask = mask;
        Length = len - 1; //exclude null terminator
    }

    uintptr_t SearchInModule() const
    {
        auto mod = (char*)GetModuleHandle(ModuleName);
        auto dosHdr = (IMAGE_DOS_HEADER*)mod;
        auto ntHdrs = (IMAGE_NT_HEADERS*)(mod + dosHdr->e_lfanew);
        auto optHdr = &ntHdrs->OptionalHeader;

        return Search(mod + optHdr->BaseOfCode, optHdr->SizeOfCode);
    }
    uintptr_t Search(const char* data, int dataLen) const
    {
        uintptr_t result = 0;
        int matches = 0;

        for (int i = 0; i < dataLen - Length; i++) {
            if (IsMatch(data + i)) {
                result = (uintptr_t)(data + i);
                matches++;
            }
        }
        if (matches != 1) {
            throw std::runtime_error("Fingerprint matches " + std::to_string(matches) + " locations, expected 1.");
        }
        return result;
    }
private:
    inline bool IsMatch(const char* data) const
    {
        //check remaining bytes, if length is not a multiple of 4
        for (int i = 0; i < Length; i++) {
            if ((data[i] ^ Pattern[i]) & Mask[i]) {
                return false;
            }
        }
        return true;
    }
};
struct HookInfo
{
    LPVOID Detour;
    LPVOID* OrigFunc;
    const char* Name;
    Fingerprint Fingerprint;
};

//Note: __thiscall can't be used directly; as a workaround, try __fastcall with an extra parameter for edx after 'this':
//__thiscall FN(void* this, int x)  ->  __fastcall FN(void* ecx, void* edx, int x)
#define DETOUR_FUNC(callConv, retType, name,  args)             \
    typedef retType (callConv *name##_FuncType)args;            \
    name##_FuncType name##_Orig;                                \
    retType callConv name##_Detour##args

#define HOOK_INFO(targetDetour, fingerprint)                    \
    { &targetDetour##_Detour, (LPVOID*)&targetDetour##_Orig, #targetDetour, fingerprint }

template <int... Offsets>
constexpr char* TraversePointers(void* ptr)
{
    for (int offset : { Offsets... }) {
        ptr = *(char**)((char*)ptr + offset);
    }
    return (char*)ptr;
}