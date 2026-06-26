// json_rpc_client.h
//
// Minimal, robust JSON-RPC 2.0 client over a raw TCP socket, tailored to the
// Renaissance Scientific NP-Drive remote interface (UM100, port 6002).
//
// The NP-Drive speaks JSON-RPC 2.0 framed as a single JSON object per response
// with NO length prefix. The manual's example does one send() + recv(128),
// which truncates larger responses and ignores partial reads. This client reads
// until the accumulated buffer parses as a complete JSON value, correlates the
// response `id`, and surfaces JSON-RPC errors as exceptions.
//
// Threading: a single call() is serialized by an internal mutex, so the object
// is safe to share, but throughput is one in-flight request at a time (which is
// all the device supports anyway).

#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>

#include "nlohmann/json.hpp"

namespace npdrive {

// Thrown for socket/connection/timeout problems (transport layer).
class TransportError : public std::runtime_error {
public:
    explicit TransportError(const std::string& what) : std::runtime_error(what) {}
};

// Thrown when the server returns a JSON-RPC `error` object.
class JsonRpcError : public std::runtime_error {
public:
    JsonRpcError(int code, const std::string& message)
        : std::runtime_error("JSON-RPC error " + std::to_string(code) + ": " + message),
          code(code) {}
    int code;
};

class JsonRpcClient {
public:
    JsonRpcClient();
    ~JsonRpcClient();

    JsonRpcClient(const JsonRpcClient&) = delete;
    JsonRpcClient& operator=(const JsonRpcClient&) = delete;

    // Open a TCP connection. host is an IPv4 dotted string or hostname.
    // Throws TransportError on failure.
    void connect(const std::string& host, uint16_t port = 6002,
                 std::chrono::milliseconds connectTimeout = std::chrono::seconds(5));

    void disconnect();
    bool isConnected() const { return socket_ != kInvalidSocket; }

    // Read/receive timeout applied to every call().
    void setTimeout(std::chrono::milliseconds timeout) { timeout_ = timeout; }

    // Invoke a method. `params` must be a JSON array (positional) or null.
    // Returns the `result` value. Throws JsonRpcError on an error response and
    // TransportError on socket trouble.
    nlohmann::json call(const std::string& method,
                        const nlohmann::json& params = nlohmann::json::array());

private:
    // Platform socket handle stored as uintptr_t so this header stays free of
    // <winsock2.h>. kInvalidSocket mirrors INVALID_SOCKET.
    using socket_t = std::uintptr_t;
    static constexpr socket_t kInvalidSocket = static_cast<socket_t>(~0);

    std::string recvOneJsonObject();

    socket_t socket_ = kInvalidSocket;
    std::int64_t nextId_ = 1;
    std::chrono::milliseconds timeout_{5000};
    std::mutex mutex_;
};

}  // namespace npdrive
