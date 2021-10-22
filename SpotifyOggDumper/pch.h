// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

#include <Windows.h>

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <string>
#include <format>
#include <regex>
#include <codecvt>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <algorithm>

#include <thread>

#include <nlohmann/json.hpp>

//undef windows macros that conflict with stdlib
#undef min
#undef max

#endif