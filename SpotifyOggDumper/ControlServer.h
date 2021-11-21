#pragma once
#include "pch.h"
#include <condition_variable>
#include <uwebsockets/App.h>
#include "StateManager.h"

//Server=CPP Client=JS
enum class MessageType
{
    TRACK_DONE  = 1,  //C -> S
    SYNC_CONFIG = 2,  //C <> S

    //Internal
    HELLO       = -1,   //Client connected
    BYE         = -2,   //Client disconnected
    SERVER_OPEN = -128  //Server listen success, message: { addr: string }
};
struct Connection;
struct Message;

using WebSocket = uWS::WebSocket<false, true, Connection>;
using MessageHandler = std::function<void(Connection*, const Message&)>;

class ControlServer
{
public:
    ControlServer(const MessageHandler& msgHandler) :
        _msgHandler(msgHandler)
    {
    }
    ~ControlServer()
    {
        Stop();
    }

    void Run();
    void Stop();
    
private:
    std::unique_ptr<uWS::App> _app;
    uWS::Loop* _loop = nullptr;
    us_listen_socket_t* _socket = nullptr;
    std::unordered_set<WebSocket*> _clients;

    std::condition_variable _doneCond;
    std::mutex _doneMutex;

    int _port = 28653;
    MessageHandler _msgHandler;
};

struct Connection
{
    WebSocket* Socket;

    void Send(const Message& msg);
};

struct Message
{
    MessageType Type;
    json Content;
    std::string BinaryContent;

    std::string Serialize() const
    {
        std::string contentStr = Content.dump();
        std::string data;
        data.reserve(5 + contentStr.size() + BinaryContent.size());

        WriteLE(data, (uint8_t)Type);
        WriteLE(data, (int32_t)contentStr.size());
        data.append(contentStr);
        data.append(BinaryContent);
        return data;
    }
    void Deserialize(std::string_view data)
    {
        auto contentLen = ReadLE<int32_t>(data, 1);

        Type = (MessageType)ReadLE<uint8_t>(data, 0);
        Content = json::parse(data.substr(5, contentLen));
        BinaryContent = data.substr(5 + contentLen);
    }

private:
    template<typename T>
    static void WriteLE(std::string& str, T value)
    {
        str.append((char*)&value, sizeof(T));
    }
    template<typename T>
    static T ReadLE(std::string_view str, size_t pos)
    {
        return *(T*)&str[pos];
    }
};