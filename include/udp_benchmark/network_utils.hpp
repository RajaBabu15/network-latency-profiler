#pragma once

#include "common.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <memory>

namespace udp_benchmark {

class NetworkUtils {
public:

    static int create_udp_socket();
    static bool configure_socket_buffers(int fd, int send_buf = config::DEFAULT_BUFFER_SIZE,
                                       int recv_buf = config::DEFAULT_BUFFER_SIZE);
    static bool set_socket_nonblocking(int fd);
    static bool set_socket_reuseaddr(int fd);


    static bool parse_address(const std::string& ip, int port, sockaddr_in& addr);
    static bool bind_socket(int fd, const sockaddr_in& addr);


    static bool is_valid_ip(const std::string& ip);
    static bool is_valid_port(int port);


    static std::string get_socket_error();
    static void print_socket_info(int fd, const std::string& description = "");
};


class Socket {
private:
    int fd_;

public:
    Socket();
    explicit Socket(int fd);
    ~Socket();


    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    int fd() const { return fd_; }
    bool is_valid() const { return fd_ >= 0; }
    void close();


    bool configure_buffers(int send_buf = config::DEFAULT_BUFFER_SIZE,
                          int recv_buf = config::DEFAULT_BUFFER_SIZE);
    bool set_nonblocking();
    bool set_reuseaddr();
    bool bind(const sockaddr_in& addr);

    ssize_t send_to(const void* data, size_t size, const sockaddr_in& dest);
    ssize_t recv_from(void* data, size_t size, sockaddr_in* src = nullptr);
};

}