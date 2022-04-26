#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace Hooks
{
    struct DataPattern
    {
        std::vector<uint8_t> Pattern;
        std::vector<uint8_t> Mask;

        DataPattern(const std::string_view& hexPattern);

        int32_t FindNext(const uint8_t* data, size_t dataLen, int32_t startPos = 0) const;
        int32_t FindPrev(const uint8_t* data, size_t dataLen, int32_t startPos, int32_t maxDist = INT_MAX) const;

        int32_t Length() const { return Pattern.size(); }
    };

    /**
     * @brief Searches for the |prefix| pattern, and returns the offset of the first match satisfying
     *        `strcmp(&data[offset + |refOffset| + ReadInt(offset + |operOffset|)], |str|) == 0`.
     *        This can be used to search the offset of an instruction loading the address of a constant string.
     */
    int32_t FindStringRef(
        const DataPattern& prefix,
        const std::string& str,
        int32_t operOffset,
        int32_t refOffset,
        const uint8_t* data, size_t dataLen
    );

    typedef void* FuncAddr;

    void Create(FuncAddr target, FuncAddr detour, FuncAddr* orig, const char* hookName = nullptr);
    void CreateApi(const wchar_t* modName, const char* funcName, FuncAddr detour, FuncAddr* orig);
    void CreatePattern(
        FuncAddr detour, FuncAddr* orig,
        const char* hookName,
        const char* modName, const DataPattern& pattern
    );
    void GetModuleCode(const char* modName, const uint8_t** codeSectionBase, size_t* codeSectionLength);

    void EnableAll();
    void DisableAll();
};

#if _WIN64
//There's only one calling convention on Windows x64 (fastcall: arg order is RCX, RDX, R8, R9, [stack])
#define DETOUR_FUNC(retType, name,  args)       \
    typedef retType (*name##_FuncType)args;     \
    name##_FuncType name##_Orig;                \
    retType name##_Detour##args

#else
//Note: __thiscall can't be used directly; as a workaround, try __fastcall with an extra parameter for edx after 'this':
//__thiscall FN(void* this, int x)  ->  __fastcall FN(void* ecx, void* edx, int x)
#define DETOUR_FUNC(callConv, retType, name,  args)             \
    typedef retType (callConv *name##_FuncType)args;            \
    name##_FuncType name##_Orig;                                \
    retType callConv name##_Detour##args

#endif

#define CREATE_HOOK(funcName, addr)             \
    Hooks::Create((Hooks::FuncAddr)(addr), &funcName##_Detour, (Hooks::FuncAddr*)&funcName##_Orig, #funcName)

#define CREATE_HOOK_PATTERN(funcName, modName, pattern) \
    Hooks::CreatePattern(&funcName##_Detour, (Hooks::FuncAddr*)&funcName##_Orig, #funcName, modName, Hooks::DataPattern(pattern))
