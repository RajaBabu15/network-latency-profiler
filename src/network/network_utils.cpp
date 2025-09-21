#include "udp_benchmark/network_utils.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <sstream>

namespace udp_benchmark {


int NetworkUtils::create_udp_socket() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket creation failed");
    }
    return fd;
}

bool NetworkUtils::configure_socket_buffers(int fd, int send_buf, int recv_buf) {
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &send_buf, sizeof(send_buf)) < 0) {
        perror("setsockopt SO_SNDBUF failed");
        return false;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recv_buf, sizeof(recv_buf)) < 0) {
        perror("setsockopt SO_RCVBUF failed");
        return false;
    }
    return true;
}

bool NetworkUtils::set_socket_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl F_GETFL failed");
        return false;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl F_SETFL O_NONBLOCK failed");
        return false;
    }
    return true;
}

bool NetworkUtils::set_socket_reuseaddr(int fd) {
    int reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        return false;
    }
    return true;
}

bool NetworkUtils::parse_address(const std::string& ip, int port, sockaddr_in& addr) {
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "Invalid IP address: " << ip << std::endl;
        return false;
    }
    return true;
}

bool NetworkUtils::bind_socket(int fd, const sockaddr_in& addr) {
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind failed");
        return false;
    }
    return true;
}

bool NetworkUtils::is_valid_ip(const std::string& ip) {
    sockaddr_in addr;
    return inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) > 0;
}

bool NetworkUtils::is_valid_port(int port) {
    return port > 0 && port < 65536;
}

std::string NetworkUtils::get_socket_error() {
    return std::string(std::strerror(errno));
}

void NetworkUtils::print_socket_info(int fd, const std::string& description) {
    if (!description.empty()) {
        std::cout << description << ":\n";
    }

    sockaddr_in addr;
    socklen_t len = sizeof(addr);

    if (getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        std::cout << "  Local address: " << ip_str << ":" << ntohs(addr.sin_port) << std::endl;
    }

    int sndbuf, rcvbuf;
    socklen_t optlen = sizeof(int);
    if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, &optlen) == 0) {
        std::cout << "  Send buffer: " << sndbuf << " bytes" << std::endl;
    }
    if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &optlen) == 0) {
        std::cout << "  Receive buffer: " << rcvbuf << " bytes" << std::endl;
    }
}


Socket::Socket() : fd_(-1) {}

Socket::Socket(int fd) : fd_(fd) {}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

void Socket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool Socket::configure_buffers(int send_buf, int recv_buf) {
    return NetworkUtils::configure_socket_buffers(fd_, send_buf, recv_buf);
}

bool Socket::set_nonblocking() {
    return NetworkUtils::set_socket_nonblocking(fd_);
}

bool Socket::set_reuseaddr() {
    return NetworkUtils::set_socket_reuseaddr(fd_);
}

bool Socket::bind(const sockaddr_in& addr) {
    return NetworkUtils::bind_socket(fd_, addr);
}

ssize_t Socket::send_to(const void* data, size_t size, const sockaddr_in& dest) {
    return sendto(fd_, data, size, 0,
                  reinterpret_cast<const sockaddr*>(&dest), sizeof(dest));
}

ssize_t Socket::recv_from(void* data, size_t size, sockaddr_in* src) {
    socklen_t src_len = src ? sizeof(*src) : 0;
    return recvfrom(fd_, data, size, 0,
                    reinterpret_cast<sockaddr*>(src), src ? &src_len : nullptr);
}

}