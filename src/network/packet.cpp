#include "udp_benchmark/packet.hpp"
#include <algorithm>

namespace udp_benchmark {


Packet::Packet(size_t size) : data_(std::max(size, sizeof(PacketHeader)), 0) {}

void Packet::set_sequence(sequence_t seq) {
    if (data_.size() >= sizeof(sequence_t)) {
        sequence_t seq_be = htobe64(seq);
        std::memcpy(data_.data(), &seq_be, sizeof(sequence_t));
    }
}

void Packet::set_timestamp(timestamp_t ts) {
    if (data_.size() >= sizeof(PacketHeader)) {
        timestamp_t ts_be = htobe64(ts);
        std::memcpy(data_.data() + sizeof(sequence_t), &ts_be, sizeof(timestamp_t));
    }
}

sequence_t Packet::get_sequence() const {
    if (data_.size() >= sizeof(sequence_t)) {
        sequence_t seq_be;
        std::memcpy(&seq_be, data_.data(), sizeof(sequence_t));
        return be64toh(seq_be);
    }
    return 0;
}

timestamp_t Packet::get_timestamp() const {
    if (data_.size() >= sizeof(PacketHeader)) {
        timestamp_t ts_be;
        std::memcpy(&ts_be, data_.data() + sizeof(sequence_t), sizeof(timestamp_t));
        return be64toh(ts_be);
    }
    return 0;
}

bool Packet::has_valid_header() const {
    return data_.size() >= sizeof(PacketHeader);
}


AckPacket::AckPacket(size_t bitmap_bytes)
    : data_(sizeof(AckHeader) + bitmap_bytes, 0) {}

void AckPacket::set_ack_sequence(sequence_t ack_seq) {
    if (data_.size() >= sizeof(sequence_t)) {
        sequence_t ack_be = htobe64(ack_seq);
        std::memcpy(data_.data(), &ack_be, sizeof(sequence_t));
    }
}

void AckPacket::set_bitmap_length(uint16_t len) {
    if (data_.size() >= sizeof(AckHeader)) {
        uint16_t len_be = htons(len);
        std::memcpy(data_.data() + sizeof(sequence_t), &len_be, sizeof(uint16_t));
    }
}

void AckPacket::set_bitmap_bit(size_t index, bool value) {
    uint8_t* bitmap = get_bitmap_data();
    size_t byte_idx = index / 8;
    int bit_idx = index % 8;

    size_t bitmap_size = data_.size() - sizeof(AckHeader);
    if (byte_idx < bitmap_size) {
        if (value) {
            bitmap[byte_idx] |= (1 << bit_idx);
        } else {
            bitmap[byte_idx] &= ~(1 << bit_idx);
        }
    }
}

sequence_t AckPacket::get_ack_sequence() const {
    if (data_.size() >= sizeof(sequence_t)) {
        sequence_t ack_be;
        std::memcpy(&ack_be, data_.data(), sizeof(sequence_t));
        return be64toh(ack_be);
    }
    return 0;
}

uint16_t AckPacket::get_bitmap_length() const {
    if (data_.size() >= sizeof(AckHeader)) {
        uint16_t len_be;
        std::memcpy(&len_be, data_.data() + sizeof(sequence_t), sizeof(uint16_t));
        return ntohs(len_be);
    }
    return 0;
}

bool AckPacket::get_bitmap_bit(size_t index) const {
    const uint8_t* bitmap = get_bitmap_data();
    size_t byte_idx = index / 8;
    int bit_idx = index % 8;

    size_t bitmap_size = data_.size() - sizeof(AckHeader);
    if (byte_idx < bitmap_size) {
        return (bitmap[byte_idx] >> bit_idx) & 1;
    }
    return false;
}

void AckPacket::clear_bitmap() {
    if (data_.size() > sizeof(AckHeader)) {
        std::memset(data_.data() + sizeof(AckHeader), 0,
                   data_.size() - sizeof(AckHeader));
    }
}


Packet PacketHandler::create_data_packet(sequence_t seq, timestamp_t ts, size_t total_size) {
    Packet packet(std::max(total_size, sizeof(PacketHeader)));
    packet.set_sequence(seq);
    packet.set_timestamp(ts);
    return packet;
}

AckPacket PacketHandler::create_ack_packet(sequence_t ack_seq,
                                          const std::vector<sequence_t>& missing_seqs,
                                          size_t window_size) {
    size_t bitmap_bytes = window_size / 8;
    AckPacket ack_packet(bitmap_bytes);

    ack_packet.set_ack_sequence(ack_seq);
    ack_packet.set_bitmap_length(bitmap_bytes);


    for (sequence_t missing : missing_seqs) {
        if (missing > ack_seq && missing <= ack_seq + window_size) {
            size_t bit_index = missing - ack_seq - 1;
            ack_packet.set_bitmap_bit(bit_index, true);
        }
    }

    return ack_packet;
}

bool PacketHandler::parse_data_packet(const uint8_t* data, size_t size,
                                     sequence_t& seq, timestamp_t& ts) {
    if (!is_valid_packet_size(size)) {
        return false;
    }

    sequence_t seq_be;
    timestamp_t ts_be;

    std::memcpy(&seq_be, data, sizeof(sequence_t));
    std::memcpy(&ts_be, data + sizeof(sequence_t), sizeof(timestamp_t));

    seq = be64toh(seq_be);
    ts = be64toh(ts_be);

    return true;
}

bool PacketHandler::parse_ack_packet(const uint8_t* data, size_t size,
                                    sequence_t& ack_seq, std::vector<sequence_t>& missing_seqs) {
    if (!is_valid_ack_size(size)) {
        return false;
    }


    sequence_t ack_be;
    uint16_t bitmap_len_be;

    std::memcpy(&ack_be, data, sizeof(sequence_t));
    std::memcpy(&bitmap_len_be, data + sizeof(sequence_t), sizeof(uint16_t));

    ack_seq = be64toh(ack_be);
    uint16_t bitmap_len = ntohs(bitmap_len_be);

    if (size < sizeof(AckHeader) + bitmap_len) {
        return false;
    }


    missing_seqs.clear();
    const uint8_t* bitmap = data + sizeof(AckHeader);

    size_t max_bits = bitmap_len * 8;
    for (size_t i = 0; i < max_bits; ++i) {
        size_t byte_idx = i / 8;
        int bit_idx = i % 8;

        if (byte_idx < bitmap_len) {
            bool bit_set = (bitmap[byte_idx] >> bit_idx) & 1;
            if (!bit_set) {
                sequence_t missing_seq = ack_seq + 1 + i;
                missing_seqs.push_back(missing_seq);
            }
        }
    }

    return true;
}

bool PacketHandler::is_valid_packet_size(size_t size) {
    return size >= sizeof(PacketHeader);
}

bool PacketHandler::is_valid_ack_size(size_t size) {
    return size >= sizeof(AckHeader);
}

}