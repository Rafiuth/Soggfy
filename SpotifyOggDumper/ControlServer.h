#pragma once
#include "pch.h"
#include <condition_variable>
#include <uwebsockets/App.h>
#include "StateManager.h"

//TODO: Redesign for "request" messages
//TODO: Checkout gRPC

//Server=CPP Client=JS
enum class MessageType
{
    SYNC_CONFIG         = 1,  //C <> S
    TRACK_META          = 2,  //C <> S
    DOWNLOAD_STATUS     = 3,  //C <> S
    OPEN_FOLDER         = 4,  //C -> S
    OPEN_FILE_PICKER    = 5,  //C <> S
    WRITE_FILE          = 6,  //C -> S
    PLAYER_STATE        = 7,  //C -> S

    //Internal
    HELLO               = -1,   //Client connected
    BYE                 = -2,   //Client disconnected
    SERVER_OPEN         = -128  //Server listen success, message: { addr: string }
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

struct Connection;

using WebSocket = uWS::WebSocket<false, true, Connection>;
using MessageHandler = std::function<void(Connection*, Message&&)>;

class ControlServer
{
public:
    ControlServer(MessageHandler msgHandler) :
        _msgHandler(msgHandler)
    {
    }
    ~ControlServer()
    {
        Stop();
    }

    void Run();
    void Stop();
    
    //Sends the specified message to all connected clients. Can be called from any thread.
    void Broadcast(const Message& msg);
    void Broadcast(MessageType type, const json& content) { Broadcast({ type, content }); }
    
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
    void Send(MessageType type, const json& content) { Send({ type, content }); }
};