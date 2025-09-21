#pragma once

#include "common.hpp"
#include "packet.hpp"
#include <map>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>

namespace udp_benchmark {


class Socket;

class ReliabilityManager {
public:
    using RetransmitCallback = std::function<void(const Packet&, const sockaddr_in&)>;
    using AckCallback = std::function<void(sequence_t, timestamp_t, timestamp_t, int)>;

private:
    std::map<sequence_t, Pending> pending_packets_;
    std::map<sequence_t, timestamp_t> send_times_;
    mutable std::mutex pending_mutex_;
    std::atomic<bool> running_{true};

    RetransmitCallback retransmit_callback_;
    AckCallback ack_callback_;


    int max_retransmits_ = 3;
    std::chrono::milliseconds ack_timeout_{1000};

public:
    explicit ReliabilityManager(RetransmitCallback retransmit_cb = nullptr,
                               AckCallback ack_cb = nullptr);
    ~ReliabilityManager();


    void add_pending_packet(sequence_t seq, timestamp_t send_time);
    void remove_pending_packet(sequence_t seq);
    bool is_packet_pending(sequence_t seq) const;


    void process_ack(sequence_t ack_seq, const std::vector<sequence_t>& missing_seqs);


    size_t get_pending_count() const;
    std::vector<sequence_t> get_pending_sequences() const;


    void set_max_retransmits(int max_retransmits) { max_retransmits_ = max_retransmits; }
    void set_ack_timeout(std::chrono::milliseconds timeout) { ack_timeout_ = timeout; }
    void set_retransmit_callback(RetransmitCallback callback) { retransmit_callback_ = callback; }
    void set_ack_callback(AckCallback callback) { ack_callback_ = callback; }


    void start();
    void stop();

private:
    void retransmit_expired_packets();
};


class AckManager {
private:
    std::unordered_map<sequence_t, timestamp_t> received_packets_;
    sequence_t highest_contiguous_ = 0;
    mutable std::mutex received_mutex_;


    int window_size_ = config::DEFAULT_WINDOW_SIZE;
    int ack_period_ = config::DEFAULT_ACK_PERIOD;
    uint64_t packets_since_ack_ = 0;

public:
    explicit AckManager(int window_size = config::DEFAULT_WINDOW_SIZE,
                       int ack_period = config::DEFAULT_ACK_PERIOD);


    bool add_received_packet(sequence_t seq, timestamp_t recv_time);
    bool is_duplicate(sequence_t seq) const;


    bool should_send_ack() const;
    AckPacket generate_ack();
    void force_ack();


    size_t get_received_count() const;
    sequence_t get_highest_contiguous() const;
    std::vector<sequence_t> get_missing_sequences(sequence_t up_to_seq) const;


    void set_window_size(int window_size) { window_size_ = window_size; }
    void set_ack_period(int ack_period) { ack_period_ = ack_period; }


    void cleanup_old_packets(sequence_t before_seq);
};


class SenderReliability {
private:
    ReliabilityManager reliability_mgr_;
    Socket* socket_;
    sockaddr_in peer_addr_;
    size_t packet_size_;

public:
    SenderReliability(Socket* socket, const sockaddr_in& peer_addr, size_t packet_size);


    bool send_packet(sequence_t seq, timestamp_t send_time);
    void process_ack_packet(const uint8_t* data, size_t size);


    void set_ack_callback(ReliabilityManager::AckCallback callback);


    size_t get_pending_count() const { return reliability_mgr_.get_pending_count(); }


    void start() { reliability_mgr_.start(); }
    void stop() { reliability_mgr_.stop(); }

private:
    void retransmit_packet(const Packet& packet, const sockaddr_in& dest);
    void handle_ack(sequence_t seq, timestamp_t send_time, timestamp_t recv_time, int retransmits);
};


class ReceiverReliability {
private:
    AckManager ack_mgr_;
    Socket* socket_;
    sockaddr_in sender_addr_;
    bool sender_addr_set_ = false;

public:
    explicit ReceiverReliability(Socket* socket,
                                int window_size = config::DEFAULT_WINDOW_SIZE,
                                int ack_period = config::DEFAULT_ACK_PERIOD);


    bool process_data_packet(const uint8_t* data, size_t size, const sockaddr_in& sender);
    void send_ack_if_needed();
    void force_ack();


    size_t get_received_count() const { return ack_mgr_.get_received_count(); }
    sequence_t get_highest_contiguous() const { return ack_mgr_.get_highest_contiguous(); }

private:
    void send_ack();
};

}