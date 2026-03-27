#pragma once
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <sys/epoll.h>
#include <unistd.h>

// Tracks all connected client fds and handles sending/detecting slow consumers.
class ClientManager {
public:
    static constexpr size_t MAX_CLIENTS    = 1024;
    static constexpr uint32_t MAX_FAILURES = 3;

    void add_client(int fd);
    void remove_client(int fd);

    // Broadcast buf of len bytes to all clients.
    // Drops slow clients (write fails MAX_FAILURES times).
    void broadcast(const uint8_t* buf, size_t len);

    size_t client_count() const { return clients_.size(); }

private:
    struct ClientState {
        int      fd;
        uint32_t fail_count{0};
    };
    std::vector<ClientState> clients_;
};