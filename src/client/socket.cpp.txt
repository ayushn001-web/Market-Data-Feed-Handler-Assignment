#include "socket.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <netdb.h>
#include "../../include/protocol.h"

MarketDataSocket::MarketDataSocket() {
    epoll_fd_ = epoll_create1(0);
}

MarketDataSocket::~MarketDataSocket() {
    disconnect();
    if (epoll_fd_ >= 0) close(epoll_fd_);
}

bool MarketDataSocket::connect(const std::string& host, uint16_t port, uint32_t timeout_ms) {
    host_ = host;
    port_ = port;
    return setup_socket(host, port);
}

bool MarketDataSocket::setup_socket(const std::string& host, uint16_t port) {
    disconnect();

    sock_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd_ < 0) { perror("socket"); return false; }

    // Configure for low latency
    set_tcp_nodelay(true);
    set_recv_buffer_size(4 * 1024 * 1024);  // 4MB recv buffer

    // Non-blocking
    int flags = fcntl(sock_fd_, F_GETFL, 0);
    fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);

    // Resolve hostname
    struct hostent* he = gethostbyname(host.c_str());
    if (!he) { fprintf(stderr, "gethostbyname failed\n"); close(sock_fd_); sock_fd_ = -1; return false; }
    memcpy(&addr.sin_addr, he->h_addr, he->h_length);

    int rc = ::connect(sock_fd_, (sockaddr*)&addr, sizeof(addr));
    if (rc < 0 && errno != EINPROGRESS) {
        perror("connect");
        close(sock_fd_); sock_fd_ = -1;
        return false;
    }

    // Wait for connection via epoll
    epoll_event ev{};
    ev.events  = EPOLLOUT | EPOLLERR;
    ev.data.fd = sock_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, sock_fd_, &ev);

    epoll_event events[1];
    int n = epoll_wait(epoll_fd_, events, 1, 5000);
    if (n <= 0) {
        fprintf(stderr, "[client] connection timed out\n");
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, sock_fd_, nullptr);
        close(sock_fd_); sock_fd_ = -1;
        return false;
    }

    int err = 0; socklen_t elen = sizeof(err);
    getsockopt(sock_fd_, SOL_SOCKET, SO_ERROR, &err, &elen);
    if (err != 0) {
        fprintf(stderr, "[client] connect error: %s\n", strerror(err));
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, sock_fd_, nullptr);
        close(sock_fd_); sock_fd_ = -1;
        return false;
    }

    // Switch to edge-triggered mode for data reading
    ev.events  = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ev.data.fd = sock_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, sock_fd_, &ev);

    connected_ = true;
    fprintf(stderr, "[client] connected to %s:%d\n", host.c_str(), port);
    return true;
}

void MarketDataSocket::disconnect() {
    if (sock_fd_ >= 0) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, sock_fd_, nullptr);
        close(sock_fd_);
        sock_fd_ = -1;
    }
    connected_ = false;
}

ssize_t MarketDataSocket::receive(void* buffer, size_t max_len) {
    if (!connected_) return -1;
    ssize_t n = recv(sock_fd_, buffer, max_len, 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;  // no data yet
        connected_ = false;
        return -1;
    }
    if (n == 0) {
        // Peer closed connection
        connected_ = false;
        return -1;
    }
    return n;
}

bool MarketDataSocket::wait_for_data(int timeout_ms) {
    epoll_event events[1];
    int n = epoll_wait(epoll_fd_, events, 1, timeout_ms);
    if (n <= 0) return false;
    if (events[0].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
        connected_ = false;
        return false;
    }
    return events[0].events & EPOLLIN;
}

bool MarketDataSocket::send_subscription(const std::vector<uint16_t>& symbol_ids) {
    if (!connected_) return false;

    // Build subscription message: 0xFF, count(2), symbol_ids...
    size_t total = 1 + 2 + symbol_ids.size() * 2;
    std::vector<uint8_t> msg(total);
    msg[0] = 0xFF;
    uint16_t count = static_cast<uint16_t>(symbol_ids.size());
    memcpy(&msg[1], &count, 2);
    for (size_t i = 0; i < symbol_ids.size(); ++i)
        memcpy(&msg[3 + i * 2], &symbol_ids[i], 2);

    ssize_t sent = send(sock_fd_, msg.data(), total, MSG_NOSIGNAL);
    return sent == static_cast<ssize_t>(total);
}

bool MarketDataSocket::set_tcp_nodelay(bool enable) {
    int flag = enable ? 1 : 0;
    return setsockopt(sock_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) == 0;
}

bool MarketDataSocket::set_recv_buffer_size(size_t bytes) {
    int sz = static_cast<int>(bytes);
    return setsockopt(sock_fd_, SOL_SOCKET, SO_RCVBUF, &sz, sizeof(sz)) == 0;
}

bool MarketDataSocket::set_socket_priority(int priority) {
    return setsockopt(sock_fd_, SOL_SOCKET, SO_PRIORITY, &priority, sizeof(priority)) == 0;
}