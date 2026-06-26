// json_rpc_client.cpp
#include "npdrive/json_rpc_client.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstring>

#pragma comment(lib, "Ws2_32.lib")

namespace npdrive {
namespace {

// Process-wide WinSock init/teardown via a static refcount-free singleton.
struct WinsockGuard {
    WinsockGuard() {
        WSADATA data;
        result = WSAStartup(MAKEWORD(2, 2), &data);
    }
    ~WinsockGuard() {
        if (result == 0) WSACleanup();
    }
    int result = -1;
};

void ensureWinsock() {
    static WinsockGuard guard;
    if (guard.result != 0) {
        throw TransportError("WSAStartup failed: " + std::to_string(guard.result));
    }
}

std::string lastSocketError(const std::string& prefix) {
    return prefix + " (WSA error " + std::to_string(WSAGetLastError()) + ")";
}

}  // namespace

JsonRpcClient::JsonRpcClient() = default;

JsonRpcClient::~JsonRpcClient() {
    disconnect();
}

void JsonRpcClient::connect(const std::string& host, uint16_t port,
                            std::chrono::milliseconds connectTimeout) {
    std::lock_guard<std::mutex> lock(mutex_);
    ensureWinsock();
    disconnect();

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* res = nullptr;
    const std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || res == nullptr) {
        throw TransportError(lastSocketError("getaddrinfo failed for " + host));
    }

    SOCKET s = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) {
        freeaddrinfo(res);
        throw TransportError(lastSocketError("socket() failed"));
    }

    // Non-blocking connect so we can enforce a connect timeout.
    u_long nonblocking = 1;
    ioctlsocket(s, FIONBIO, &nonblocking);

    int rc = ::connect(s, res->ai_addr, static_cast<int>(res->ai_addrlen));
    freeaddrinfo(res);

    if (rc == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) {
        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(s, &writeSet);
        timeval tv{};
        tv.tv_sec = static_cast<long>(connectTimeout.count() / 1000);
        tv.tv_usec = static_cast<long>((connectTimeout.count() % 1000) * 1000);
        rc = ::select(0, nullptr, &writeSet, nullptr, &tv);
        if (rc <= 0) {
            closesocket(s);
            throw TransportError("connect to " + host + ":" + portStr + " timed out");
        }
        int soError = 0;
        int len = sizeof(soError);
        getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soError), &len);
        if (soError != 0) {
            closesocket(s);
            throw TransportError("connect failed (SO_ERROR " + std::to_string(soError) + ")");
        }
    } else if (rc == SOCKET_ERROR) {
        closesocket(s);
        throw TransportError(lastSocketError("connect failed"));
    }

    // Back to blocking; per-call timeouts come from SO_RCVTIMEO/SO_SNDTIMEO.
    nonblocking = 0;
    ioctlsocket(s, FIONBIO, &nonblocking);

    socket_ = static_cast<socket_t>(s);
}

void JsonRpcClient::disconnect() {
    if (socket_ != kInvalidSocket) {
        closesocket(static_cast<SOCKET>(socket_));
        socket_ = kInvalidSocket;
    }
}

std::string JsonRpcClient::recvOneJsonObject() {
    std::string buffer;
    char chunk[512];
    for (;;) {
        int n = ::recv(static_cast<SOCKET>(socket_), chunk, sizeof(chunk), 0);
        if (n == 0) {
            throw TransportError("connection closed by peer");
        }
        if (n == SOCKET_ERROR) {
            const int err = WSAGetLastError();
            if (err == WSAETIMEDOUT) {
                throw TransportError("response timed out");
            }
            throw TransportError(lastSocketError("recv failed"));
        }
        buffer.append(chunk, static_cast<size_t>(n));

        // The device sends one complete JSON object. Once the buffer parses, we
        // have the whole message. accept() does not throw and is cheap enough
        // for the small responses this device produces.
        if (nlohmann::json::accept(buffer)) {
            return buffer;
        }
    }
}

nlohmann::json JsonRpcClient::call(const std::string& method, const nlohmann::json& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (socket_ == kInvalidSocket) {
        throw TransportError("not connected");
    }

    const std::int64_t id = nextId_++;
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params.is_null() ? nlohmann::json::array() : params},
        {"id", id},
    };
    const std::string payload = request.dump();

    // Apply timeouts for this exchange.
    DWORD ms = static_cast<DWORD>(timeout_.count());
    setsockopt(static_cast<SOCKET>(socket_), SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&ms), sizeof(ms));
    setsockopt(static_cast<SOCKET>(socket_), SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&ms), sizeof(ms));

    size_t sent = 0;
    while (sent < payload.size()) {
        int n = ::send(static_cast<SOCKET>(socket_), payload.data() + sent,
                       static_cast<int>(payload.size() - sent), 0);
        if (n == SOCKET_ERROR) {
            throw TransportError(lastSocketError("send failed"));
        }
        sent += static_cast<size_t>(n);
    }

    const std::string raw = recvOneJsonObject();
    nlohmann::json response = nlohmann::json::parse(raw, nullptr, /*allow_exceptions=*/false);
    if (response.is_discarded()) {
        throw TransportError("malformed JSON response: " + raw);
    }

    if (response.contains("error") && !response["error"].is_null()) {
        const auto& err = response["error"];
        const int code = err.value("code", 0);
        const std::string msg = err.value("message", std::string("unknown"));
        throw JsonRpcError(code, msg);
    }

    // id correlation: tolerate string-or-number ids from the firmware.
    if (response.contains("id")) {
        const auto& rid = response["id"];
        bool matches = (rid.is_number_integer() && rid.get<std::int64_t>() == id) ||
                       (rid.is_string() && rid.get<std::string>() == std::to_string(id));
        if (!matches) {
            throw TransportError("response id mismatch (expected " + std::to_string(id) + ")");
        }
    }

    return response.contains("result") ? response["result"] : nlohmann::json();
}

}  // namespace npdrive
