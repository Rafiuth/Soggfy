#include "ControlServer.h"
#include "Utils/Log.h"

//https://github.com/joyent/libuv/issues/1089#issuecomment-33316312
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "userenv.lib")

void ControlServer::Run()
{
    _loop = uWS::Loop::get();
    _app = std::make_unique<uWS::App>();
    CreateIdleTimer();

    _app->ws<Connection>("/sgf_ctrl", {
        .compression = uWS::CompressOptions::DISABLED,
        .maxPayloadLength = 1024 * 1024 * 4,
        .idleTimeout = 32,
        .maxBackpressure = 1024 * 1024 * 8,
        .closeOnBackpressureLimit = false,
        .resetIdleTimeoutOnSend = false,
        .sendPingsAutomatically = true,

        .upgrade = [](uWS::HttpResponse<false>* res, uWS::HttpRequest* req, us_socket_context_t* context) {
            auto secKey = req->getHeader("sec-websocket-key");
            auto secProto = req->getHeader("sec-websocket-protocol");
            auto secExts = req->getHeader("sec-websocket-extensions");

            res->writeStatus("101 Switching Protocols");
            res->writeHeader("Access-Control-Allow-Origin", "*");
            res->upgrade(Connection(), secKey, secProto, secExts, context);
        },
        .open = [&](WebSocket* ws) {
            LogInfo("Incomming connection from {}@{}", ws->getRemoteAddressAsText(), (void*)ws);
            ws->getUserData()->Socket = ws;
            _clients.insert(ws);
            _msgHandler(ws->getUserData(), { MessageType::HELLO });
        },
        .message = [&](WebSocket* ws, std::string_view data, uWS::OpCode dataType) {
            try {
                if (dataType != uWS::BINARY) {
                    throw std::exception("Binary message expected");
                }
                Message msg;
                msg.Deserialize(data);
                _msgHandler(ws->getUserData(), std::move(msg));
            } catch (std::exception& ex) {
                LogWarn("Error while processing message from {}: {}", (void*)ws, ex.what());
            }
        },
        .close = [&](WebSocket* ws, int code, std::string_view reason) {
            LogInfo("Closing connection from {}@{} (code={} reason={})", ws->getRemoteAddressAsText(), (void*)ws, code, reason);
            _msgHandler(ws->getUserData(), { MessageType::BYE });
            _clients.erase(ws);
            ws->getUserData()->Socket = nullptr;
        }
    });

    int attemptNum = 0;
    std::function<void(us_listen_socket_t*)> listenHandler = [&](auto socket) {
        if (socket) {
            _socket = socket;
            LogInfo("Control server listening on port {}", _port);
        } else {
            if (attemptNum > 64) {
                throw std::exception("Failed to open control server.");
            }
            LogDebug("Failed to listen on port {}, trying again...", _port);
            attemptNum++;
            _port++;
            _app->listen("127.0.0.1", _port, listenHandler);
        }
    };
    _app->listen("127.0.0.1", _port, LIBUS_LISTEN_EXCLUSIVE_PORT, listenHandler);
    _app->run(); //will block until _socket and all connections are closed

    std::lock_guard lock(_doneMutex);
    _doneCond.notify_all();
    _app = nullptr;
}
void ControlServer::Stop()
{
    if (_socket) {
        _loop->defer([&]() {
            //create a copy because the closeHandler fired by end() will invalidate the iterator
            auto clients = _clients;
            for (auto ws : clients) {
                ws->end(1001, "Server is shutting down");
            }
            us_timer_close(_idleTimer);
            us_listen_socket_close(0, _socket);
            _socket = nullptr;
        });
        std::unique_lock lock(_doneMutex);
        _doneCond.wait(lock);
    }
}

void ControlServer::CreateIdleTimer()
{
    _idleTimer = us_create_timer((us_loop_t*)_loop, 1, sizeof(ControlServer*));
    *(ControlServer**)us_timer_ext(_idleTimer) = this;

    us_timer_set(_idleTimer, [](us_timer_t* timer) {
        auto sv = *(ControlServer**)us_timer_ext(timer);
        if (sv->_clients.size() == 0) {
            sv->_msgHandler(nullptr, { MessageType::IDLE });
        }
    }, 1000, 1000);
}

void SendData(WebSocket* socket, const std::string& data, uWS::OpCode opcode = uWS::BINARY)
{
    if (socket->send(data, opcode) != WebSocket::SendStatus::SUCCESS) {
        LogWarn("Failed to send message to {}", (void*)socket);
    }
}

void ControlServer::Broadcast(const Message& msg)
{
    _loop->defer([this, data = std::move(msg.Serialize())]() {
        for (auto ws : _clients) {
            SendData(ws, data);
        }
    });
}

void Connection::Send(const Message& msg)
{
    SendData(Socket, msg.Serialize());
}