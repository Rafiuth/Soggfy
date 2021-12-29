#pragma once
#include "pch.h"

class StateManager
{
public:
    virtual void ReceiveAudioData(const std::string& playbackId, const char* data, int length) = 0;
    virtual void OnTrackCreated(const std::string& playbackId) = 0;
    virtual void OnTrackDone(const std::string& playbackId) = 0;
    //Marks the playback as incomplete (seeked or skipped), this will cancel it's download.
    virtual void DiscardTrack(const std::string& playbackId, const std::string& reason = "") = 0;

    virtual bool OverridePlaybackSpeed(double& speed) = 0;
    virtual bool IsUrlBlocked(std::wstring_view url) = 0;

    virtual void RunControlServer() = 0;
    //Closes any open files and the control server.
    virtual void Shutdown() = 0;

    static std::unique_ptr<StateManager> New(const std::filesystem::path& dataDir);
};