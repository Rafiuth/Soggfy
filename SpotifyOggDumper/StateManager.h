#pragma once
#include "pch.h"

// Ugh, why did I make this stuff all virtual?
class StateManager
{
public:
    virtual void ReceiveAudioData(const std::string& playbackId, const char* data, int length) = 0;
    virtual double GetPlaySpeed() = 0;

    virtual bool IsUrlBlocked(std::wstring_view url) = 0;

    virtual void RunControlServer() = 0;
    //Closes any open files and the control server.
    virtual void Shutdown() = 0;

    static std::unique_ptr<StateManager> New(const fs::path& dataDir, const fs::path& moduleDir);
};