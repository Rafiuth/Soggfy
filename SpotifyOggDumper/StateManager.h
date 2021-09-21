#pragma once
#include "pch.h"
#include "OggDefs.h"

class StateManager
{
public:
    virtual void OnTrackCreated(const std::string& playbackId, const std::string& trackId) = 0;
    virtual void OnTrackOpened(const std::string& playbackId) = 0;
    virtual void OnTrackClosed(const std::string& playbackId) = 0;

    virtual void OnTrackSeeked(const std::string& playbackId) = 0;

    virtual void ReceiveOggPage(uintptr_t syncId, ogg_page* page) = 0;

    virtual void UpdateAccToken(const std::string& token) = 0;

    //OOP in C++ kinda sucks lol

    static std::unique_ptr<StateManager> New(const std::filesystem::path& dataDir);
};