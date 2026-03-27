#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// Non-blocking TCP client socket with epoll and auto-reconnect.
class MarketDataSocket {
public:
    MarketDataSocket();
    ~MarketDataSocket();

    bool connect(const std::string& host, uint16_t port, uint32_t timeout_ms = 5000);
    void disconnect();

    // Non-blocking receive. Returns bytes read, 0 on EAGAIN, -1 on error/disconnect.
    ssize_t receive(void* buffer, size_t max_len);

    // Send subscription request for given symbol IDs
    bool send_subscription(const std::vector<uint16_t>& symbol_ids);

    // Wait for data to be available (epoll). Returns true if data ready.
    bool wait_for_data(int timeout_ms = 100);

    bool is_connected() const { return connected_; }
    int  fd() const { return sock_fd_; }

    bool set_tcp_nodelay(bool enable);
    bool set_recv_buffer_size(size_t bytes);
    bool set_socket_priority(int priority);

private:
    bool setup_socket(const std::string& host, uint16_t port);

    int  sock_fd_{-1};
    int  epoll_fd_{-1};
    bool connected_{false};

    std::string host_;
    uint16_t    port_{0};
};