// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdint>

#include <filesystem>
#include <iostream>

#define JSON_DIAGNOSTICS _DEBUG
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using namespace nlohmann;

//undef windows macros that conflict with stdlib
#undef min
#undef max

#endif