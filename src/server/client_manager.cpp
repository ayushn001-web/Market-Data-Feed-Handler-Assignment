#include "client_manager.h"
#include <sys/socket.h>
#include <cerrno>
#include <cstdio>
#include <algorithm>

void ClientManager::add_client(int fd) {
    if (clients_.size() >= MAX_CLIENTS) {
        fprintf(stderr, "[server] max clients reached, rejecting fd=%d\n", fd);
        close(fd);
        return;
    }
    clients_.push_back({fd, 0});
    fprintf(stderr, "[server] client connected fd=%d (total=%zu)\n", fd, clients_.size());
}

void ClientManager::remove_client(int fd) {
    auto it = std::find_if(clients_.begin(), clients_.end(),
        [fd](const ClientState& c) { return c.fd == fd; });
    if (it != clients_.end()) {
        close(it->fd);
        clients_.erase(it);
        fprintf(stderr, "[server] client disconnected fd=%d (remaining=%zu)\n", fd, clients_.size());
    }
}

void ClientManager::broadcast(const uint8_t* buf, size_t len) {
    std::vector<int> to_remove;

    for (auto& c : clients_) {
        ssize_t sent = 0;
        size_t total = 0;
        // Try to send full message; non-blocking so handle partial sends
        while (total < len) {
            sent = send(c.fd, buf + total, len - total, MSG_NOSIGNAL | MSG_DONTWAIT);
            if (sent <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Slow consumer — count failure
                    c.fail_count++;
                    if (c.fail_count >= MAX_FAILURES) {
                        fprintf(stderr, "[server] dropping slow client fd=%d\n", c.fd);
                        to_remove.push_back(c.fd);
                    }
                } else {
                    to_remove.push_back(c.fd);
                }
                break;
            }
            total += static_cast<size_t>(sent);
            c.fail_count = 0;  // reset on successful send
        }
    }

    for (int fd : to_remove)
        remove_client(fd);
}