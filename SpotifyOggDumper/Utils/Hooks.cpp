#include "Hooks.h"

#include <Windows.h>
#include <MinHook.h>
#include <cassert>

#include "Log.h"

namespace Hooks
{
    DataPattern::DataPattern(const std::string_view& hexPattern)
    {
        //Parse pattern in form of "A1 B2 C3 ??"
        assert((hexPattern.length() + 1) % 3 == 0);
        int32_t numBytes = (hexPattern.length() + 1) / 3;

        const auto ParseDigit = [](char d, uint8_t& byte, uint8_t& mask, int shift) {
            /**/ if (d >= '0' && d <= '9') d = d - '0';
            else if (d >= 'A' && d <= 'F') d = d - 'A' + 10;
            else if (d >= 'a' && d <= 'f') d = d - 'a' + 10;
            else { assert(d == '?'); return; }

            byte |= d << shift;
            mask |= 0xF << shift;
        };
        Pattern.resize(numBytes);
        Mask.resize(numBytes);

        for (int32_t i = 0; i < numBytes; i++) {
            uint8_t byte = 0, mask = 0;
            ParseDigit(hexPattern[i * 3ull + 0], byte, mask, 4);
            ParseDigit(hexPattern[i * 3ull + 1], byte, mask, 0);

            Pattern[i] = byte;
            Mask[i] = mask;
        }
    }

    static bool MaskedEquals(const uint8_t* data, const uint8_t* patt, const uint8_t* mask, int32_t len)
    {
        for (int32_t i = 0; i < len; i++) {
            if ((data[i] ^ patt[i]) & mask[i]) {
                return false;
            }
        }
        return true;
    }

    int32_t DataPattern::FindNext(const uint8_t* data, size_t dataLen, int32_t startPos) const
    {
        auto patt = Pattern.data();
        auto mask = Mask.data();
        int32_t pattLen = Length();

        for (int32_t i = startPos; i < dataLen - pattLen; i++) {
            if (MaskedEquals(&data[i], patt, mask, pattLen)) {
                return i;
            }
        }
        return -1;
    }
    int32_t DataPattern::FindPrev(const uint8_t* data, size_t dataLen, int32_t startPos, int32_t maxDist) const
    {
        auto patt = Pattern.data();
        auto mask = Mask.data();
        int32_t pattLen = Length();
        int32_t minPos = max(startPos - maxDist, 0);
        int32_t maxPos = min(dataLen - pattLen - 1, startPos);

        for (int32_t i = maxPos; i >= minPos; i--) {
            if (MaskedEquals(&data[i], patt, mask, pattLen)) {
                return i;
            }
        }
        return -1;
    }

    int32_t FindStringRef(
        const DataPattern& prefix,
        const std::string& str,
        int32_t operOffset,
        int32_t refOffset,
        const uint8_t* data, size_t dataLen)
    {
        for (int32_t i = 0; (i = prefix.FindNext(data, dataLen, i)) >= 0; ) {
            int32_t strOffset = *(int32_t*)(&data[i + operOffset]);
            auto refedStr = &data[i + refOffset + strOffset];
            if (refedStr >= data && refedStr < data + dataLen && str.compare((char*)refedStr) == 0) {
                return i;
            }
            i += prefix.Length();
        }
        return -1;
    }

    static volatile bool g_mhInitialized = false;

    static void MH_Check(MH_STATUS status, const char* hookName = nullptr)
    {
        if (status != MH_OK) {
            char buf[128];
            sprintf_s(buf, "Failed to create hook '%s': %s", hookName ? hookName : "<unnamed>", MH_StatusToString(status));
            throw std::exception(buf);
        }
    }

    void Create(FuncAddr target, FuncAddr detour, FuncAddr* orig, const char* hookName)
    {
        if (!g_mhInitialized) {
            g_mhInitialized = true;
            MH_Check(MH_Initialize());
        }
        MH_Check(MH_CreateHook(target, detour, orig), hookName);
    }

    void CreateApi(const wchar_t* modName, const char* funcName, FuncAddr detour, FuncAddr* orig)
    {
        MH_Check(MH_CreateHookApi(modName, funcName, detour, orig));
    }

    void CreatePattern(
        FuncAddr detour, FuncAddr* orig,
        const char* hookName,
        const char* modName, const DataPattern& pattern)
    {
        const uint8_t* codeBase;
        size_t codeLength;
        GetModuleCode(modName, &codeBase, &codeLength);

        int32_t offset = pattern.FindNext(codeBase, codeLength);
        if (offset < 0) {
            LogDebug("[Hooks::CreatePattern] No matches found for {}", hookName);
            MH_Check(MH_ERROR_FUNCTION_NOT_FOUND, hookName);
        }
        LogDebug("[Hooks::CreatePattern] {} found at {}", hookName, (void*)(codeBase + offset));
        Create((FuncAddr)(codeBase + offset), detour, orig, hookName);
    }
    
    void GetModuleCode(const char* modName, const uint8_t** codeSectionBase, size_t* codeSectionLength)
    {
        auto mod = (uint8_t*)GetModuleHandleA(modName);
        auto dosHdr = (IMAGE_DOS_HEADER*)mod;
        auto ntHdrs = (IMAGE_NT_HEADERS*)(mod + dosHdr->e_lfanew);
        auto optHdr = &ntHdrs->OptionalHeader;
        *codeSectionBase = mod + optHdr->BaseOfCode;
        *codeSectionLength = optHdr->SizeOfCode;
    }

    void EnableAll()
    {
        MH_Check(MH_EnableHook(MH_ALL_HOOKS));
    }
    void DisableAll()
    {
        MH_Check(MH_DisableHook(MH_ALL_HOOKS));
        if (g_mhInitialized) {
            g_mhInitialized = false;
            MH_Uninitialize();
        }
    }
}