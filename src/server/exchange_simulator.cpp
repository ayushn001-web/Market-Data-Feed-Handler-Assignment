#include "exchange_simulator.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <chrono>
#include <thread>
#include "../../include/protocol.h"

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

ExchangeSimulator::ExchangeSimulator(uint16_t port, size_t num_symbols)
    : port_(port), tick_gen_(num_symbols) {}

ExchangeSimulator::~ExchangeSimulator() {
    stop();
    if (server_fd_ >= 0) close(server_fd_);
    if (epoll_fd_ >= 0) close(epoll_fd_);
}

void ExchangeSimulator::start() {
    // Create TCP server socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) { perror("socket"); return; }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblocking(server_fd_);

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port_);

    if (bind(server_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); return;
    }
    if (listen(server_fd_, SOMAXCONN) < 0) {
        perror("listen"); return;
    }

    // Set up epoll
    epoll_fd_ = epoll_create1(0);
    epoll_event ev{};
    ev.events  = EPOLLIN;
    ev.data.fd = server_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &ev);

    running_.store(true);
    tick_thread_ = std::thread(&ExchangeSimulator::tick_loop, this);

    fprintf(stderr, "[server] listening on port %d, %zu symbols\n",
            port_, tick_gen_.num_symbols());
}

void ExchangeSimulator::run() {
    constexpr int MAX_EVENTS = 64;
    epoll_event events[MAX_EVENTS];

    while (running_.load()) {
        int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100 /*ms*/);
        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == server_fd_) {
                handle_new_connection();
            } else {
                // On server side we only care about disconnections
                if (events[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP))
                    handle_client_disconnect(events[i].data.fd);
            }
        }
    }
}

void ExchangeSimulator::stop() {
    running_.store(false);
    if (tick_thread_.joinable()) tick_thread_.join();
}

void ExchangeSimulator::set_tick_rate(uint32_t tps) {
    tick_rate_.store(tps);
}

void ExchangeSimulator::enable_fault_injection(bool e) {
    fault_injection_.store(e);
}

void ExchangeSimulator::handle_new_connection() {
    while (true) {
        int client_fd = accept(server_fd_, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            perror("accept");
            break;
        }
        set_nonblocking(client_fd);

        // Disable Nagle's on server side too
        int flag = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        // Watch for disconnection
        epoll_event ev{};
        ev.events  = EPOLLIN | EPOLLHUP | EPOLLRDHUP;
        ev.data.fd = client_fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev);

        client_mgr_.add_client(client_fd);
    }
}

void ExchangeSimulator::handle_client_disconnect(int fd) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    client_mgr_.remove_client(fd);
}

void ExchangeSimulator::tick_loop() {
    const size_t num_sym = tick_gen_.num_symbols();
    uint8_t buf[proto::MAX_MSG_SIZE];
    uint16_t sym_idx = 0;

    while (running_.load()) {
        uint32_t rate = tick_rate_.load();
        // sleep_ns = 1e9 / rate
        auto sleep_ns = std::chrono::nanoseconds(
            static_cast<long long>(1e9 / std::max(rate, 1u)));

        auto t0 = std::chrono::steady_clock::now();

        uint32_t seq = seq_counter_.fetch_add(1, std::memory_order_relaxed);
        size_t len = tick_gen_.generate(sym_idx, buf, seq);

        if (len > 0) {
            // Fault injection: randomly corrupt 1% of sequence numbers
            if (fault_injection_.load() && (seq % 100 == 7)) {
                // Skip a sequence number to simulate gap
                seq_counter_.fetch_add(5, std::memory_order_relaxed);
            }
            client_mgr_.broadcast(buf, len);
        }

        sym_idx = static_cast<uint16_t>((sym_idx + 1) % num_sym);

        // Occasionally send heartbeat
        if (seq % 5000 == 0) {
            proto::HeartbeatMsg hb{};
            hb.hdr.msg_type     = proto::MSG_HEARTBEAT;
            hb.hdr.seq_num      = seq_counter_.fetch_add(1, std::memory_order_relaxed);
            hb.hdr.timestamp_ns = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
            hb.hdr.symbol_id    = 0;
            hb.checksum = proto::compute_checksum(&hb, sizeof(hb) - sizeof(uint32_t));
            client_mgr_.broadcast(reinterpret_cast<uint8_t*>(&hb), sizeof(hb));
        }

        auto elapsed = std::chrono::steady_clock::now() - t0;
        if (elapsed < sleep_ns)
            std::this_thread::sleep_for(sleep_ns - elapsed);
    }
}