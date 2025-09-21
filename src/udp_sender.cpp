#include "udp_benchmark/network_utils.hpp"
#include "udp_benchmark/reliability.hpp"
#include "udp_benchmark/congestion_control.hpp"
#include "udp_benchmark/stats.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace udp_benchmark;

int main(int argc, char** argv) {
    if (argc < 7) {
        std::cerr << "Usage: " << argv[0] << " <recv_ip> <port> <msg_size> <rate_msgs/s> <total_msgs> <log.csv>\n";
        std::cerr << "Parameters:\n";
        std::cerr << "  recv_ip:     IP address of the receiver (e.g., 127.0.0.1 for localhost)\n";
        std::cerr << "  port:        UDP port number (e.g., 9000)\n";
        std::cerr << "  msg_size:    Total message size in bytes (minimum 16 for headers)\n";
        std::cerr << "  rate_msgs/s: Target sending rate in messages per second\n";
        std::cerr << "  total_msgs:  Total number of messages to send\n";
        std::cerr << "  log.csv:     Path to output CSV log file\n";
        return 1;
    }

    std::string recv_ip = argv[1];
    int port = std::atoi(argv[2]);
    int msg_size = std::atoi(argv[3]);
    double rate = std::atof(argv[4]);
    uint64_t total_msgs = std::strtoull(argv[5], nullptr, 10);
    std::string logfile = argv[6];

    if (msg_size < config::MIN_MESSAGE_SIZE) {
        std::cerr << "Error: msg_size must be at least " << config::MIN_MESSAGE_SIZE << " bytes for headers\n";
        return 1;
    }

    if (!NetworkUtils::is_valid_ip(recv_ip) || !NetworkUtils::is_valid_port(port)) {
        std::cerr << "Error: Invalid IP address or port\n";
        return 1;
    }

    std::cout << "UDP Sender configuration:\n";
    std::cout << "  Target: " << recv_ip << ":" << port << "\n";
    std::cout << "  Message size: " << msg_size << " bytes\n";
    std::cout << "  Target rate: " << static_cast<int>(rate) << " msgs/sec\n";
    std::cout << "  Total messages: " << total_msgs << "\n";
    std::cout << "  Logging to: " << logfile << "\n";

    Socket socket(NetworkUtils::create_udp_socket());
    if (!socket.is_valid()) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    socket.configure_buffers();
    socket.set_nonblocking();
    socket.set_reuseaddr();

    sockaddr_in peer_addr;
    if (!NetworkUtils::parse_address(recv_ip, port, peer_addr)) {
        std::cerr << "Failed to parse address\n";
        return 1;
    }

    LatencyLogger logger(logfile);
    if (!logger.is_open()) {
        std::cerr << "Failed to open log file\n";
        return 1;
    }

    SenderReliability reliability(&socket, peer_addr, msg_size);
    EnhancedCongestionController congestion_ctrl(1000, 5000, true);
    StatsCollector stats;
    RateLimiter rate_limiter(rate);
    ProgressReporter progress(total_msgs);

    reliability.set_ack_callback([&](sequence_t seq, timestamp_t send_time, timestamp_t recv_time, int retransmits) {
        logger.log_sender_data(seq, send_time, recv_time, retransmits);
        stats.add_packet_received(msg_size);
        congestion_ctrl.packet_acked();
    });

    std::atomic<bool> running{true};
    std::thread ack_thread([&]() {
        uint8_t buf[config::MAX_PACKET_SIZE];
        while (running) {
            ssize_t n = socket.recv_from(buf, sizeof(buf));
            if (n > 0) {
                reliability.process_ack_packet(buf, n);
                congestion_ctrl.on_ack_received_with_stats();
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    });

    reliability.start();
    stats.start_collection();

    std::cout << "Starting to send messages...\n";

    for (sequence_t seq = 1; seq <= total_msgs; ++seq) {
        while (!congestion_ctrl.can_send()) {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }

        rate_limiter.wait_for_next_send();

        timestamp_t send_time = get_timestamp_ns();
        if (reliability.send_packet(seq, send_time)) {
            congestion_ctrl.packet_sent();
            stats.add_packet_sent(msg_size);
            progress.increment();
            
            if (progress.get_progress_percentage() >= static_cast<int>(seq * 10 / total_msgs) * 10) {
                progress.print_progress();
            }
        }
    }

    std::cout << "\n\nAll messages sent! Waiting for final ACKs...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    running = false;
    ack_thread.join();
    reliability.stop();
    stats.end_collection();

    std::cout << "Sender finished. Sent " << total_msgs << " messages.\n";
    std::cout << "Check " << logfile << " for results.\n";

    stats.print_final_summary();

    return 0;
}