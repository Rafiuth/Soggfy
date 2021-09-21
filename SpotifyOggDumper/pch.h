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

#include <string>
#include <format>
#include <regex>
#include <codecvt>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

#include <thread>

#include <nlohmann/json.hpp>

#define TAGLIB_STATIC
#include <taglib/attachedpictureframe.h>
#include <taglib/vorbisfile.h>

#endif