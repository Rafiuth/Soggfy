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

    void SearchInModule(std::vector<LPVOID>& results) const
    {
        auto mod = (char*)GetModuleHandle(ModuleName);
        auto dosHdr = (IMAGE_DOS_HEADER*)mod;
        auto ntHdrs = (IMAGE_NT_HEADERS*)(mod + dosHdr->e_lfanew);
        auto optHdr = &ntHdrs->OptionalHeader;

        return Search(results, mod + optHdr->BaseOfCode, optHdr->SizeOfCode);
    }
    void Search(std::vector<LPVOID>& results, const char* data, int dataLen) const
    {
        for (int i = 0; i < dataLen - Length; i++) {
            //check if there's a match at i
            for (int j = 0; j < Length; j++) {
                if ((data[i + j] ^ Pattern[j]) & Mask[j]) {
                    goto TryAgain;
                }
            }
            results.push_back((LPVOID)(data + i));
        TryAgain:;
        }
    }
};
struct HookInfo
{
    LPVOID Detour;
    LPVOID* OrigFunc;
    Fingerprint TargetFingerprint;
};

//Note: __thiscall can't be used directly; as a workaround, try __fastcall with an extra parameter for edx after 'this':
//__thiscall FN(void* this, int x)  ->  __fastcall FN(void* ecx, void* edx, int x)
#define DETOUR_FUNC(CALL_CONV, RET_TYPE, NAME,  SIG)            \
    typedef RET_TYPE (CALL_CONV *NAME##_FuncType)SIG;           \
    NAME##_FuncType NAME##_Orig;                                \
    RET_TYPE CALL_CONV NAME##_Detour##SIG

template <int... Offsets>
constexpr void* TraversePointers(void* ptr)
{
    for (int offset : { Offsets... }) {
        ptr = *(char**)((char*)ptr + offset);
    }
    return ptr;
}