#include "udp_benchmark/network_utils.hpp"
#include "udp_benchmark/reliability.hpp"
#include "udp_benchmark/stats.hpp"
#include <iostream>

using namespace udp_benchmark;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <listen_port> <logfile.csv>\n";
        return 1;
    }

    int port = std::atoi(argv[1]);
    std::string logfile = argv[2];

    if (!NetworkUtils::is_valid_port(port)) {
        std::cerr << "Error: Invalid port number\n";
        return 1;
    }

    Socket socket(NetworkUtils::create_udp_socket());
    if (!socket.is_valid()) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    socket.set_reuseaddr();

    sockaddr_in addr;
    if (!NetworkUtils::parse_address("0.0.0.0", port, addr)) {
        std::cerr << "Failed to parse address\n";
        return 1;
    }

    if (!socket.bind(addr)) {
        std::cerr << "Failed to bind socket\n";
        return 1;
    }

    LatencyLogger logger(logfile);
    if (!logger.is_open()) {
        std::cerr << "Failed to open log file\n";
        return 1;
    }

    ReceiverReliability reliability(&socket);
    StatsCollector stats;

    std::cout << "UDP Receiver listening on port " << port << " (logging to " << logfile << ")\n";

    stats.start_collection();

    uint8_t buf[config::MAX_PACKET_SIZE];
    sockaddr_in sender_addr;

    while (true) {
        ssize_t n = socket.recv_from(buf, sizeof(buf), &sender_addr);
        if (n <= 0) continue;

        timestamp_t recv_time = get_timestamp_ns();

        sequence_t seq;
        timestamp_t send_ts;
        if (PacketHandler::parse_data_packet(buf, n, seq, send_ts)) {
            bool is_new = reliability.process_data_packet(buf, n, sender_addr);
            
            if (is_new) {
                logger.log_receiver_data(seq, recv_time, send_ts);
                stats.add_packet_received(n);
                stats.add_latency_measurement(send_ts, recv_time);
                
                if (stats.should_report_progress()) {
                    std::cout << "Received packets: " << stats.get_throughput_stats().packets_received 
                              << " (latest seq: " << seq << ")\r" << std::flush;
                }
            }

            reliability.send_ack_if_needed();
        }
    }

    stats.end_collection();
    return 0;
}