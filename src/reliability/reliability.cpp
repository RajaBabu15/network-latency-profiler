#include "udp_benchmark/reliability.hpp"
#include "udp_benchmark/network_utils.hpp"
#include <algorithm>
#include <chrono>

namespace udp_benchmark {


ReliabilityManager::ReliabilityManager(RetransmitCallback retransmit_cb, AckCallback ack_cb)
    : retransmit_callback_(retransmit_cb), ack_callback_(ack_cb) {}

ReliabilityManager::~ReliabilityManager() {
    stop();
}

void ReliabilityManager::add_pending_packet(sequence_t seq, timestamp_t send_time) {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_packets_[seq] = Pending(seq, send_time, 0);
    send_times_[seq] = send_time;
}

void ReliabilityManager::remove_pending_packet(sequence_t seq) {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_packets_.erase(seq);
    send_times_.erase(seq);
}

bool ReliabilityManager::is_packet_pending(sequence_t seq) const {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    return pending_packets_.find(seq) != pending_packets_.end();
}

void ReliabilityManager::process_ack(sequence_t ack_seq, const std::vector<sequence_t>& missing_seqs) {
    std::lock_guard<std::mutex> lock(pending_mutex_);


    std::vector<sequence_t> to_remove;
    for (auto& [seq, pending] : pending_packets_) {
        if (seq <= ack_seq) {
            to_remove.push_back(seq);


            if (ack_callback_) {
                timestamp_t send_time = send_times_.count(seq) ? send_times_[seq] : pending.send_ts_ns;
                ack_callback_(seq, send_time, get_timestamp_ns(), pending.retransmits);
            }
        }
    }

    for (sequence_t seq : to_remove) {
        pending_packets_.erase(seq);
        send_times_.erase(seq);
    }


    for (sequence_t missing : missing_seqs) {
        auto it = pending_packets_.find(missing);
        if (it != pending_packets_.end()) {
            it->second.retransmits++;


            if (retransmit_callback_) {
                Packet packet = PacketHandler::create_data_packet(
                    missing, it->second.send_ts_ns, config::MIN_MESSAGE_SIZE);
                sockaddr_in dummy_addr{};
                retransmit_callback_(packet, dummy_addr);
            }
        }
    }
}

size_t ReliabilityManager::get_pending_count() const {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    return pending_packets_.size();
}

std::vector<sequence_t> ReliabilityManager::get_pending_sequences() const {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    std::vector<sequence_t> sequences;
    for (const auto& [seq, _] : pending_packets_) {
        sequences.push_back(seq);
    }
    return sequences;
}

void ReliabilityManager::start() {
    running_.store(true);
}

void ReliabilityManager::stop() {
    running_.store(false);
}

void ReliabilityManager::retransmit_expired_packets() {


}


AckManager::AckManager(int window_size, int ack_period)
    : window_size_(window_size), ack_period_(ack_period) {}

bool AckManager::add_received_packet(sequence_t seq, timestamp_t recv_time) {
    std::lock_guard<std::mutex> lock(received_mutex_);


    if (received_packets_.find(seq) != received_packets_.end()) {
        return false;
    }

    received_packets_[seq] = recv_time;
    packets_since_ack_++;


    while (received_packets_.find(highest_contiguous_ + 1) != received_packets_.end()) {
        highest_contiguous_++;
    }

    return true;
}

bool AckManager::is_duplicate(sequence_t seq) const {
    std::lock_guard<std::mutex> lock(received_mutex_);
    return received_packets_.find(seq) != received_packets_.end();
}

bool AckManager::should_send_ack() const {
    return packets_since_ack_ >= static_cast<uint64_t>(ack_period_);
}

AckPacket AckManager::generate_ack() {
    std::lock_guard<std::mutex> lock(received_mutex_);


    std::vector<sequence_t> missing_seqs;
    sequence_t window_end = highest_contiguous_ + window_size_;

    for (sequence_t seq = highest_contiguous_ + 1; seq <= window_end; ++seq) {
        if (received_packets_.find(seq) == received_packets_.end()) {
            missing_seqs.push_back(seq);
        }
    }

    packets_since_ack_ = 0;
    return PacketHandler::create_ack_packet(highest_contiguous_, missing_seqs, window_size_);
}

void AckManager::force_ack() {
    packets_since_ack_ = ack_period_;
}

size_t AckManager::get_received_count() const {
    std::lock_guard<std::mutex> lock(received_mutex_);
    return received_packets_.size();
}

sequence_t AckManager::get_highest_contiguous() const {
    return highest_contiguous_;
}

std::vector<sequence_t> AckManager::get_missing_sequences(sequence_t up_to_seq) const {
    std::lock_guard<std::mutex> lock(received_mutex_);
    std::vector<sequence_t> missing;

    for (sequence_t seq = highest_contiguous_ + 1; seq <= up_to_seq; ++seq) {
        if (received_packets_.find(seq) == received_packets_.end()) {
            missing.push_back(seq);
        }
    }

    return missing;
}

void AckManager::cleanup_old_packets(sequence_t before_seq) {
    std::lock_guard<std::mutex> lock(received_mutex_);
    auto it = received_packets_.begin();
    while (it != received_packets_.end()) {
        if (it->first < before_seq) {
            it = received_packets_.erase(it);
        } else {
            ++it;
        }
    }
}


SenderReliability::SenderReliability(Socket* socket, const sockaddr_in& peer_addr, size_t packet_size)
    : socket_(socket), peer_addr_(peer_addr), packet_size_(packet_size) {


    reliability_mgr_.set_retransmit_callback(
        [this](const Packet& packet, const sockaddr_in& dest) {
            this->retransmit_packet(packet, dest);
        });


    reliability_mgr_.set_ack_callback(
        [this](sequence_t seq, timestamp_t send_time, timestamp_t recv_time, int retransmits) {
            this->handle_ack(seq, send_time, recv_time, retransmits);
        });
}

bool SenderReliability::send_packet(sequence_t seq, timestamp_t send_time) {

    Packet packet = PacketHandler::create_data_packet(seq, send_time, packet_size_);
    ssize_t sent = socket_->send_to(packet.data(), packet.size(), peer_addr_);

    if (sent > 0) {
        reliability_mgr_.add_pending_packet(seq, send_time);
        return true;
    }
    return false;
}

void SenderReliability::process_ack_packet(const uint8_t* data, size_t size) {
    sequence_t ack_seq;
    std::vector<sequence_t> missing_seqs;

    if (PacketHandler::parse_ack_packet(data, size, ack_seq, missing_seqs)) {
        reliability_mgr_.process_ack(ack_seq, missing_seqs);
    }
}

void SenderReliability::set_ack_callback(ReliabilityManager::AckCallback callback) {
    reliability_mgr_.set_ack_callback(callback);
}

void SenderReliability::retransmit_packet(const Packet& packet, const sockaddr_in& /* dest */) {
    socket_->send_to(packet.data(), packet.size(), peer_addr_);
}

void SenderReliability::handle_ack(sequence_t /* seq */, timestamp_t /* send_time */, timestamp_t /* recv_time */, int /* retransmits */) {
    // TODO: Implement ACK handling logic if needed
}


ReceiverReliability::ReceiverReliability(Socket* socket, int window_size, int ack_period)
    : ack_mgr_(window_size, ack_period), socket_(socket) {}

bool ReceiverReliability::process_data_packet(const uint8_t* data, size_t size,
                                            const sockaddr_in& sender) {
    sequence_t seq;
    timestamp_t send_ts;

    if (!PacketHandler::parse_data_packet(data, size, seq, send_ts)) {
        return false;
    }


    if (!sender_addr_set_) {
        sender_addr_ = sender;
        sender_addr_set_ = true;
    }

    timestamp_t recv_time = get_timestamp_ns();
    bool is_new = ack_mgr_.add_received_packet(seq, recv_time);


    send_ack_if_needed();

    return is_new;
}

void ReceiverReliability::send_ack_if_needed() {
    if (ack_mgr_.should_send_ack() && sender_addr_set_) {
        send_ack();
    }
}

void ReceiverReliability::force_ack() {
    ack_mgr_.force_ack();
    if (sender_addr_set_) {
        send_ack();
    }
}

void ReceiverReliability::send_ack() {
    AckPacket ack = ack_mgr_.generate_ack();
    socket_->send_to(ack.data(), ack.size(), sender_addr_);
}

}