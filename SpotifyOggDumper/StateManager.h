#pragma once
#include "pch.h"

class StateManager
{
public:
    virtual void OnTrackCreated(const std::string& playbackId, const std::string& trackId) = 0;
    virtual void OnTrackOpened(const std::string& playbackId, int positionMs) = 0;
    virtual void OnTrackClosed(const std::string& playbackId, const std::string& reason) = 0;

    virtual void OnTrackSeeked(const std::string& playbackId) = 0;

    virtual void ReceiveAudioData(const std::string& playbackId, const char* data, int length) = 0;

    virtual void UpdateAccToken(const std::string& token) = 0;

    //Closes any open files. called during uninstall.
    virtual void Shutdown() = 0;

    //OOP in C++ kinda sucks lol

    static std::unique_ptr<StateManager> New(const std::filesystem::path& dataDir);
};